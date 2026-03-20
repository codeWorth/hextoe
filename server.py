import random
import secrets
from datetime import datetime, timedelta
from typing import Optional, Union

from fastapi import FastAPI, Header, HTTPException, Response
from passlib.context import CryptContext
from pydantic import BaseModel
from sqlalchemy import and_, func, or_
from sqlalchemy.orm import aliased
from sqlmodel import Field, Session, SQLModel, create_engine, select


# -- DB setup --

DATABASE_URL = "mysql+pymysql://user:password@localhost/hextoe"
engine = create_engine(DATABASE_URL)

app = FastAPI()

SESSION_TTL_HOURS = 48
pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")
MOVE_ALREADY_TAKEN = "ALREADY_TAKEN"
MOVE_TOO_FAR = "TOO_FAR"
MAX_MOVE_DIST = 7

@app.on_event("startup")
def on_startup():
    SQLModel.metadata.create_all(engine)


# -- Table definitions --

# User always has some session_id, but the TTL may indicate it has expired.
class User(SQLModel, table=True):
    user_id: str = Field(primary_key=True)
    username: str = Field(unique=True)
    password_hash: str
    session_id: str = Field(index=True)
    session_ttl: datetime


# is_complete with empty winner ID indicates the game ended in a draw.
# If one of the player_id slots is empty, and the is_public field is true,
# then this game is waiting for the 2nd player to join.
class Game(SQLModel, table=True):
    game_id: str = Field(primary_key=True)
    player_id_1: Optional[str] = Field(default=None, foreign_key="user.user_id")
    player_id_2: Optional[str] = Field(default=None, foreign_key="user.user_id")
    winner_id: Optional[str] = Field(default=None, foreign_key="user.user_id")
    is_complete: bool = Field(default=False)
    creation_time: datetime = Field(default_factory=datetime.utcnow)
    is_public: bool = Field(default=True)


class Move(SQLModel, table=True):
    game_id: str = Field(foreign_key="game.game_id", primary_key=True)
    move_index: int = Field(primary_key=True)
    a: bool  # row parity (0 or 1)
    r: int   # row
    c: int   # column


# -- Helper functions --

def new_id_str() -> str:
    return secrets.token_urlsafe(32)


def new_session_ttl() -> datetime:
    return datetime.utcnow() + timedelta(hours=SESSION_TTL_HOURS)


# Return the User for the given session_id, or raise 403 if the ID is invalid
# (could be not present or expired)
def require_valid_session(session_id: str, db: Session) -> User:
    user = db.exec(select(User).where(User.session_id == session_id)).first()
    if user is None or user.session_ttl <= datetime.utcnow():
        raise HTTPException(status_code=403)
    return user


def other_value(a, b, v):
    if v == a:
        return b
    if v == b:
        return a

    raise HTTPException(status_code=500)


# Return 0 if no one has won yet, 1 if player 1 got 6 in a row, or 2 if player
# 2 got 6 in a row. If both players have somehow gotten 6 in a row, we just
# return 1, since we check that player first.
def check_winner(moves: list[Move]) -> int:
    return 0


# Return the user_id of the player whose turn it is
def get_current_player_id(game: Game, num_moves: int) -> Optional[str]:
    if game.player_id_1 is None or game.player_id_2 is None:
        return None
    if num_moves == 0:
        return game.player_id_1
    if ((num_moves - 1) // 2) % 2 == 0:
        return game.player_id_2
    return game.player_id_1


# Construct the game state dict for sending to the client
def build_game_state(game: Game, moves: list[Move]) -> dict:
    if game.is_complete:
        current_player = None
    else:
        current_player = get_current_player_id(game, len(moves))
    return {
        "player_id_1": game.player_id_1,
        "player_id_2": game.player_id_2,
        "winner_id": game.winner_id,
        "current_player_id": current_player,
        "moves": [{"a": int(m.a), "r": m.r, "c": m.c} for m in moves],
    }


# Return a failure reason string, or None if the move is valid
def validate_move(a: int, r: int, c: int, existing_moves: list[Move]) -> Optional[str]:
    # We can't overwrite an existing X or O
    for m in existing_moves:
        if int(m.a) == a and m.r == r and m.c == c:
            return MOVE_ALREADY_TAKEN

    # We can't place an X or O too far away from the other Xs and Os
    if len(existing_moves) > 0:
        close_enough = False
        for m in existing_moves:
            dA = 0 if int(m.a) == a else 1
            dR = abs(r - m.r)
            dC = abs(c - m.c)
            if dA + dR * 2 + dC <= MAX_MOVE_DIST:
                close_enough = True
                break
        if not close_enough:
            return MOVE_TOO_FAR

    return None


# -- JSON request schemas --

class UserInfoBody(BaseModel):
    username: str
    password: str


class UpdateUserBody(BaseModel):
    username: Optional[str] = None
    password: Optional[str] = None


class MoveBody(BaseModel):
    a: int
    r: int
    c: int


# -- JSON response schemas --

class MoveResponse(BaseModel):
    a: int
    r: int
    c: int


class GameStateResponse(BaseModel):
    player_id_1: Optional[str]
    player_id_2: Optional[str]
    winner_id: Optional[str]
    current_player_id: Optional[str]
    moves: list[MoveResponse]


class MoveFailureResponse(BaseModel):
    failure_reason: str


class GameSummary(BaseModel):
    game_id: str
    opponent_username: Optional[str]
    creation_time: datetime
    total_moves: int
    winner_username: Optional[str]


class UserProfileResponse(BaseModel):
    username: str
    games: list[GameSummary]
    current_game_ids: list[str]


# -- HTTP Endpoints --

# Create new user
@app.post("/user", response_model=str)
def create_user(body: UserInfoBody):
    with Session(engine) as db:
        if db.exec(select(User).where(User.username == body.username)).first():
            raise HTTPException(status_code=409)
        user = User(
            user_id=new_id_str(),
            username=body.username,
            password_hash=pwd_context.hash(body.password),
            session_id=new_id_str(),
            session_ttl=new_session_ttl(),
        )
        db.add(user)
        db.commit()
        return user.session_id


# Update info of existing user
@app.put("/user", response_model=str)
def update_user(body: UpdateUserBody, session_id: str = Header(...)):
    if body.username is None and body.password is None:
        raise HTTPException(status_code=400)
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        if body.username is not None:
            user.username = body.username
        if body.password is not None:
            user.password_hash = pwd_context.hash(body.password)
        user.session_id = new_id_str()
        user.session_ttl = new_session_ttl()
        db.add(user)
        db.commit()
        return user.session_id


# Delete user account
@app.delete("/user", status_code=204)
def delete_user(session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        db.delete(user)
        db.commit()


# Get existing user info
@app.get("/user/{user_id}", response_model=UserProfileResponse)
def get_user(user_id: str):
    with Session(engine) as db:
        user = db.get(User, user_id)
        if user is None:
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
                Opponent.username.label("opponent_username"),
                Winner.username.label("winner_username"),
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
            game_list.append({
                "game_id": row.game_id,
                "opponent_username": row.opponent_username,
                "creation_time": row.creation_time,
                "total_moves": row.total_moves,
                "winner_username": row.winner_username,
            })

        return {
            "username": user.username,
            "games": game_list,
            "current_game_ids": current_game_ids,
        }


# User login. This generates a new session id each time, meaning any other
# session will become invalidated
@app.post("/user/login", response_model=str)
def login(body: UserInfoBody):
    with Session(engine) as db:
        user = db.exec(select(User).where(User.username == body.username)).first()
        if user is None or not pwd_context.verify(body.password, user.password_hash):
            raise HTTPException(status_code=404)
        user.session_id = new_id_str()
        user.session_ttl = new_session_ttl()
        db.add(user)
        db.commit()
        return user.session_id


# User logout. This simply sets TTL to now so that it is considered expired
@app.post("/user/logout", status_code=204)
def logout(session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        user.session_ttl = datetime.utcnow()
        db.add(user)
        db.commit()


# Join a game which is looking for a player, or create a public game for
# others to join. If we already are looking for a game, we just return that
# game id.
@app.post("/game/join", response_model=str)
def join_game(session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        # Check if user is already in an in-progress game
        existing = db.exec(
            select(Game).where(
                Game.is_complete == False,
                (Game.player_id_1 == user.user_id) | (Game.player_id_2 == user.user_id),
                Game.is_public == True,
            )
        ).first()
        if existing:
            return existing.game_id

        # We aren't in a game. Look for an open public game to join (oldest first)
        open_game = db.exec(
            select(Game).where(
                Game.is_public == True,
                or_(Game.player_id_1 == None, Game.player_id_2 == None),
            )
            .order_by(Game.creation_time.asc())
        ).first()

        if open_game:
            if open_game.player_id_1 is None:
                open_game.player_id_1 = user.user_id
            else:
                open_game.player_id_2 = user.user_id
            db.add(open_game)
            db.commit()
            return open_game.game_id

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


# Create a private game. This game ID can be manually shared by users to invite
# someone else to play directly.
@app.post("/game/private", response_model=str)
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


# Get game state
@app.get("/game/{game_id}", response_model=GameStateResponse)
def get_game(game_id: str, session_id: str = Header(...)):
    with Session(engine) as db:
        require_valid_session(session_id, db)

        game = db.get(Game, game_id)
        if game is None:
            raise HTTPException(status_code=404)

        moves = db.exec(
            select(Move).where(Move.game_id == game_id).order_by(Move.move_index)
        ).all()
        return build_game_state(game, moves)


# Submit a move
@app.post("/game/{game_id}", response_model=Union[GameStateResponse, MoveFailureResponse])
def make_move(game_id: str, body: MoveBody, session_id: str = Header(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        game = db.get(Game, game_id)
        if game is None:
            raise HTTPException(status_code=404)

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
        db.add(game)
        db.commit()

        return build_game_state(game, moves)


# Undo the most recent move
@app.post("/game/undo/{game_id}", response_model=GameStateResponse)
def undo_move(game_id: str, session_id: str = Header(...)):
    with Session(engine) as db:
        require_valid_session(session_id, db)

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
        return build_game_state(game, moves[:-1])
