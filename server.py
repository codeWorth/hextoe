import random
from datetime import datetime
from typing import Union

from fastapi import FastAPI, Header, HTTPException
from fastapi.responses import FileResponse
from sqlalchemy import and_, func, or_
from sqlalchemy.orm import aliased
from sqlmodel import Session, SQLModel, create_engine, select

from db_schemas import Game, Move, User
from json_schemas import (
    GameStateResponse,
    GameSummary,
    MoveBody,
    MoveFailureResponse,
    UpdateUserBody,
    UserInfoBody,
    UserProfileResponse,
    MyUserProfileResponse,
)
from util import (
    build_game_state,
    check_winner,
    get_current_player_id,
    get_uname_safe,
    get_uuname_safe,
    new_id_str,
    new_session_ttl,
    hash_password,
    other_value,
    verify_password,
    require_valid_session,
    validate_move,
    winner_index,
)


# -- DB setup --

DATABASE_URL = "mysql+pymysql://hextoe@localhost/hextoe"
engine = create_engine(DATABASE_URL)

app = FastAPI()


@app.on_event("startup")
def on_startup():
    SQLModel.metadata.create_all(engine)


# -- Webpages --

@app.get("/")
def serve_home():
    return FileResponse("home.html")


@app.get("/login")
def serve_login():
    return FileResponse("login.html")


@app.get("/game/{game_id}")
def serve_game(game_id: str):
    return FileResponse("game.html")


@app.get("/src/hex.js")
def serve_hex_js():
    return FileResponse("hex.js")


@app.get("/test")
def serve_test():
    return FileResponse("test.html")


# -- HTTP Rest Endpoints --

# Create new user
@app.post("/api/user", response_model=str)
def create_user(body: UserInfoBody):
    with Session(engine) as db:
        if db.exec(select(User).where(User.username == body.username)).first():
            raise HTTPException(status_code=409)
        user = User(
            user_id=new_id_str(),
            username=body.username,
            password_hash=hash_password(body.password),
            session_id=new_id_str(),
            session_ttl=new_session_ttl(),
        )
        db.add(user)
        db.commit()
        return user.session_id


@app.post("/api/anon_user", response_model=str)
def create_anon_user():
    with Session(engine) as db:
        user = User(
            user_id=new_id_str(),
            username=new_id_str(),
            password_hash="",
            session_id=new_id_str(),
            session_ttl=new_session_ttl(),
            is_anon=True
        )
        db.add(user)
        db.commit()
        return user.session_id


# Update info of existing user
@app.put("/api/user", response_model=str)
def update_user(body: UpdateUserBody, session_id: str = Header(...)):
    if body.username is None and body.password is None:
        raise HTTPException(status_code=400)
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        if user.is_anon:
            return user.session_id
        if body.username is not None:
            user.username = body.username
        if body.password is not None:
            user.password_hash = hash_password(body.password)
        user.session_id = new_id_str()
        user.session_ttl = new_session_ttl()
        db.add(user)
        db.commit()
        return user.session_id


# Delete user account
@app.delete("/api/user", status_code=204)
def delete_user(session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        if user.is_anon:
            return
        user.is_deleted = True
        user.session_ttl = datetime.utcnow()
        db.add(user)
        db.commit()


# Get current user info
@app.get("/api/user", response_model=MyUserProfileResponse)
def get_current_user(session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        return _get_user(user.user_id, db) | {"user_id": user.user_id}


# Get existing user info
@app.get("/api/user/{user_id}", response_model=UserProfileResponse)
def get_user(user_id: str):
    with Session(engine) as db:
        return _get_user(user_id, db)


# Private func to get the user by user id
def _get_user(user_id: str, db: Session):
    user = db.get(User, user_id)
    if user is None or user.is_deleted:
        raise HTTPException(status_code=404)

    Opponent = aliased(User)
    Winner = aliased(User)

    # This query gets all of the fields we're interested in one shot via
    # joining on the User and Move tables. The somewhat confusing outerjoin
    # on Opponent is just saying that we want to join against the user which
    # is not our current user. So if our user id is in the `player_id_1`
    # field, we join on `player_id_2`, and vice versa.
    rows = db.exec(
        select(
            Game.game_id,
            Game.is_complete,
            Game.creation_time,
            func.any_value(Opponent.username).label("opponent_username"),
            func.any_value(Opponent.is_deleted).label("opponent_deleted"),
            func.any_value(Opponent.is_anon).label("opponent_anon"),
            func.any_value(Winner.username).label("winner_username"),
            func.any_value(Winner.is_deleted).label("winner_deleted"),
            func.any_value(Winner.is_anon).label("winner_anon"),
            func.count(Move.move_index).label("total_moves"),
        )
        .where(
            Game.player_id_1 != None,
            Game.player_id_2 != None,
            (Game.player_id_1 == user_id) | (Game.player_id_2 == user_id),
        )
        .outerjoin(
            Opponent,
            or_(
                and_(Game.player_id_1 == user_id, Opponent.user_id == Game.player_id_2),
                and_(Game.player_id_2 == user_id, Opponent.user_id == Game.player_id_1),
            ),
        )
        .outerjoin(Winner, Winner.user_id == Game.winner_id)
        .outerjoin(Move, Move.game_id == Game.game_id)
        .group_by(Game.game_id)
    ).all()

    current_game_ids = []
    game_list = []
    for row in rows:
        if not row.is_complete:
            current_game_ids.append(row.game_id)
        opponent_name = get_uname_safe(row.opponent_username, row.opponent_deleted,
                                        row.opponent_anon)
        winner_name = get_uname_safe(row.winner_username, row.winner_deleted,
                                        row.winner_anon)
        game_list.append({
            "game_id": row.game_id,
            "opponent_username": opponent_name,
            "creation_time": row.creation_time,
            "total_moves": row.total_moves,
            "winner_username": winner_name,
        })

    return {
        "username": get_uuname_safe(user),
        "games": game_list,
        "current_game_ids": current_game_ids,
    }


# User login. This generates a new session id each time, meaning any other
# session will become invalidated
@app.post("/api/user/login", response_model=str)
def login(body: UserInfoBody):
    with Session(engine) as db:
        user = db.exec(select(User).where(User.username == body.username)).first()
        if user is None or not verify_password(body.password, user.password_hash):
            raise HTTPException(status_code=404)
        user.session_id = new_id_str()
        user.session_ttl = new_session_ttl()
        db.add(user)
        db.commit()
        return user.session_id


# User logout. This simply sets TTL to now so that it is considered expired
@app.post("/api/user/logout", status_code=204)
def logout(session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        user.session_ttl = datetime.utcnow()
        db.add(user)
        db.commit()


def _do_join_game(open_game: Game, user: User, db: Session) -> str:
    if open_game.player_id_1 is None:
        open_game.player_id_1 = user.user_id
    else:
        open_game.player_id_2 = user.user_id
    db.add(open_game)
    db.commit()
    return open_game.game_id


# Join a game which is looking for a player, or create a public game for
# others to join. If we already are looking for a game, we just return that
# game id.
@app.post("/api/game/join", response_model=str)
def join_game(session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        # Check if user is already in an in-progress game, join the most recent
        # one if so
        existing = db.exec(
            select(Game).where(
                Game.is_complete == False,
                (Game.player_id_1 == user.user_id) | (Game.player_id_2 == user.user_id),
                Game.is_public == True,
            )
            .order_by(Game.creation_time.desc())
        ).first()
        if existing:
            return existing.game_id

        # We aren't in a game. Look for an open public game to join (oldest first)
        open_game = db.exec(
            select(Game).where(
                Game.is_public == True,
                Game.is_complete == False,
                or_(Game.player_id_1 == None, Game.player_id_2 == None),
            )
            .order_by(Game.creation_time.asc())
        ).first()

        if open_game:
            return _do_join_game(open_game, user, db)

        # No open game, create a new one. We randomly decide if we'll be player 1
        # or player 2
        game = Game(game_id=new_id_str())
        if random.random() < 0.5:
            game.player_id_1 = user.user_id
        else:
            game.player_id_2 = user.user_id
        db.add(game)
        db.commit()
        return game.game_id


@app.post("/api/game/join/{game_id}", response_model=str)
def join_specific_game(game_id: str, session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        game = db.exec(
            select(Game).where(
                Game.game_id == game_id,
                Game.is_complete == False,
                (Game.player_id_1 == None) | (Game.player_id_2 == None),
                or_(Game.player_id_1 == None, Game.player_id_1 != user.user_id),
                or_(Game.player_id_2 == None, Game.player_id_2 != user.user_id),
            )
        ).first()
        if not game:
            raise HTTPException(status_code=404)

        return _do_join_game(game, user, db)


# Create a private game. This game ID can be manually shared by users to invite
# someone else to play directly.
@app.post("/api/game/private", response_model=str)
def create_private_game(session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        existing = db.exec(
            select(Game).where(
                Game.is_public == False,
                Game.is_complete == False,
                or_(
                    and_(Game.player_id_1 == user.user_id, Game.player_id_2 == None),
                    and_(Game.player_id_2 == user.user_id, Game.player_id_1 == None),
                ),
            )
        ).first()
        if existing:
            return existing.game_id

        game = Game(game_id=new_id_str(), is_public=False)
        if random.random() < 0.5:
            game.player_id_1 = user.user_id
        else:
            game.player_id_2 = user.user_id
        db.add(game)
        db.commit()
        return game.game_id


def _get_player_names(game: Game, db: Session):
    p1_name = None
    p2_name = None
    if game.player_id_1:
        u = db.get(User, game.player_id_1)
        if u: p1_name = get_uuname_safe(u)
    if game.player_id_2:
        u = db.get(User, game.player_id_2)
        if u: p2_name = get_uuname_safe(u)
    return p1_name, p2_name


# Get game state
@app.get("/api/game/{game_id}", response_model=GameStateResponse)
def get_game(game_id: str, session_id: str = Header(None)):
    with Session(engine) as db:
        user_id = None
        if session_id is not None:
            user = require_valid_session(session_id, db)
            user_id = user.user_id

        game = db.get(Game, game_id)
        if game is None:
            raise HTTPException(status_code=404)

        moves = db.exec(
            select(Move).where(Move.game_id == game_id).order_by(Move.move_index)
        ).all()
        p1_name, p2_name = _get_player_names(game, db)
        return build_game_state(game, moves, user_id, p1_name, p2_name)


# Submit a move
@app.post("/api/game/{game_id}", response_model=Union[GameStateResponse, MoveFailureResponse])
def make_move(game_id: str, body: MoveBody, session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        game = db.get(Game, game_id)
        if game is None:
            raise HTTPException(status_code=404)

        if game.is_complete:
            raise HTTPException(status_code=400)

        moves = db.exec(
            select(Move).where(Move.game_id == game_id).order_by(Move.move_index)
        ).all()

        current_player = get_current_player_id(game, len(moves))
        if user.user_id != current_player:
            raise HTTPException(status_code=400)

        failure = validate_move(body.a, body.r, body.c, moves)
        if failure:
            return {"failure_reason": failure}

        new_move = Move(
            game_id=game_id,
            move_index=len(moves),
            a=bool(body.a),
            r=body.r,
            c=body.c,
        )
        db.add(new_move)
        moves.append(new_move)

        winner = check_winner(moves)
        if winner == 1:
            game.winner_id = game.player_id_1
            game.is_complete = True
        elif winner == 2:
            game.winner_id = game.player_id_2
            game.is_complete = True

        game.move_time = datetime.utcnow()
        db.add(game)
        db.commit()

        p1_name, p2_name = _get_player_names(game, db)
        return build_game_state(game, moves, user.user_id, p1_name, p2_name)


# Undo the most recent move
@app.post("/api/game/undo/{game_id}", response_model=GameStateResponse)
def undo_move(game_id: str, session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        game = db.get(Game, game_id)
        if game is None:
            raise HTTPException(status_code=404)

        if game.is_complete:
            raise HTTPException(status_code=400)

        moves = db.exec(
            select(Move).where(Move.game_id == game_id).order_by(Move.move_index)
        ).all()

        current_player = get_current_player_id(game, len(moves))
        if len(moves) % 2 == 0 or user.user_id != current_player:
            raise HTTPException(status_code=400)

        db.delete(moves[-1])
        db.commit()
        p1_name, p2_name = _get_player_names(game, db)
        return build_game_state(game, moves[:-1], user.user_id, p1_name, p2_name)


# Get a list of recent active games.
# Returns up to 25 games where the game has at least started (both player ids are
# present), and the game is public
@app.post("/api/games")
def get_games():
    with Session(engine) as db:
        Player1 = aliased(User)
        Player2 = aliased(User)
        games = db.exec(
            select(
                Game.game_id,
                Game.is_complete,
                Game.player_id_1,
                Game.player_id_2,
                Game.winner_id,
                func.any_value(Player1.username).label("p1_uname"),
                func.any_value(Player1.is_deleted).label("p1_deleted"),
                func.any_value(Player1.is_anon).label("p1_anon"),
                func.any_value(Player2.username).label("p2_uname"),
                func.any_value(Player2.is_deleted).label("p2_deleted"),
                func.any_value(Player2.is_anon).label("p2_anon"),
                func.count(Move.move_index).label("total_moves"),
            ).where(
                Game.player_id_1 != None,
                Game.player_id_2 != None,
                Game.is_public == True
            )
            .outerjoin(Player1, Game.player_id_1 == Player1.user_id)
            .outerjoin(Player2, Game.player_id_2 == Player2.user_id)
            .outerjoin(Move, Move.game_id == Game.game_id)
            .group_by(Game.game_id)
            .order_by(Game.move_time.desc())
            .limit(25)
        ).all()

        return [{
            "game_id": game.game_id,
            "p1_uname": get_uname_safe(game.p1_uname, game.p1_deleted, game.p1_anon),
            "p2_uname": get_uname_safe(game.p2_uname, game.p2_deleted, game.p2_anon),
            "total_moves": game.total_moves,
            "is_complete": game.is_complete,
            "winner_index": winner_index(game.player_id_1, game.player_id_2, game.winner_id)
        } for game in games]


# Forfeit a game
@app.post("/api/game/forfeit/{game_id}", status_code=204)
def forfeit_game(game_id: str, session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        game = db.exec(
            select(Game).where(
                Game.game_id == game_id,
                Game.is_complete == False,
                (Game.player_id_1 == user.user_id) | (Game.player_id_2 == user.user_id),
            )
        ).first()
        if game is None:
            raise HTTPException(status_code=404)

        game.is_complete = True
        if game.player_id_1 is not None and game.player_id_2 is not None:
            game.winner_id = other_value(
                game.player_id_1, game.player_id_2, user.user_id
            )
        db.add(game)
        db.commit()
