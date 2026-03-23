import asyncio
import random
import time
import threading
from datetime import datetime
from types import SimpleNamespace
from typing import Union, Optional
import os


from fastapi import Cookie, FastAPI, HTTPException, Query, Response
from fastapi.responses import FileResponse
from sqlalchemy import and_, func, or_, update, not_
from sqlalchemy.orm import aliased
from sqlmodel import Session, SQLModel, create_engine, select

from db_schemas import BestMove, ChatMessage, Game, Move, User, UNAME_MAX, CHATMESSAGE_MAX
from json_schemas import (
    BotMoveResponse,
    ChatMessageBody,
    ChatResponse,
    GameStateResponse,
    MoveBody,
    MoveFailureResponse,
    UpdateUserBody,
    UserInfoBody,
    UserProfileResponse,
    GameListResponse,
)
import bot as _bot
from util import (
    build_game_state,
    check_winner,
    get_current_player_id,
    get_uname_safe,
    get_uuname_safe,
    is_player_one,
    new_id_str,
    new_session_ttl,
    hash_password,
    other_value,
    verify_password,
    require_valid_session,
    validate_move,
    winner_index,
    get_game_with_unames,
    update_game_last_req,
    prune_afk_games,
    get_min_req,
    prune_delete_game_conds,
    prune_finish_game_conds,
    MAX_MESSAGES,
    BOT_UID,
)


# -- DB setup --

DATABASE_URL = os.getenv("HEXTOE_DB_URL")
if not DATABASE_URL:
    raise RuntimeError("HEXTOE_DB_URL is not set in the environment")

engine = create_engine(DATABASE_URL)

app = FastAPI()


async def _prune_loop():
    while True:
        await asyncio.sleep(120)
        with Session(engine) as db:
            prune_afk_games(db)


def _eval_loop():
    while True:
        time.sleep(0.25)
        _bot.proc_eval_task(engine)


@app.on_event("startup")
def on_startup():
    SQLModel.metadata.create_all(engine)
    with Session(engine) as db:
        if db.get(User, BOT_UID) is None:
            db.add(User(
                user_id=BOT_UID,
                username="Bot",
                password_hash=b"",
                session_id=new_id_str(),
                session_ttl=datetime.utcnow(),
            ))
            db.commit()
    asyncio.get_event_loop().create_task(_prune_loop())
    _bot._loop = asyncio.get_event_loop()
    threading.Thread(target=_eval_loop, daemon=True).start()


def _set_session_cookie(response: Response, session_id: str, session_ttl: datetime):
    max_age = int((session_ttl - datetime.utcnow()).total_seconds())
    response.set_cookie(
        key="session_id",
        value=session_id,
        httponly=True,
        samesite="strict",
        max_age=max_age,
        path="/",
    )


def _clear_session_cookie(response: Response):
    response.delete_cookie(key="session_id", path="/")


# -- HTTP Rest Endpoints --

# Create new user
@app.post("/api/user", status_code=204)
def create_user(body: UserInfoBody, response: Response):
    with Session(engine) as db:
        if db.exec(select(User).where(User.username == body.username)).first():
            raise HTTPException(status_code=409)
        if len(body.username) > UNAME_MAX:
            raise HTTPException(status_code=400)
        user = User(
            user_id=new_id_str(),
            username=body.username,
            password_hash=hash_password(body.password),
            session_id=new_id_str(),
            session_ttl=new_session_ttl(),
        )
        db.add(user)
        db.commit()
        _set_session_cookie(response, user.session_id, user.session_ttl)


@app.post("/api/anon_user", status_code=204)
def create_anon_user(response: Response):
    with Session(engine) as db:
        user = User(
            user_id=new_id_str(),
            username=new_id_str(),
            password_hash=b"",
            session_id=new_id_str(),
            session_ttl=new_session_ttl(),
            is_anon=True
        )
        db.add(user)
        db.commit()
        _set_session_cookie(response, user.session_id, user.session_ttl)


# Update info of existing user
@app.put("/api/user", status_code=204)
def update_user(body: UpdateUserBody, response: Response, session_id: str = Cookie(...)):
    if body.username is None and body.password is None and body.bot_assist is None:
        raise HTTPException(status_code=400)
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        if user.is_anon:
            raise HTTPException(status_code=400)
        updated_uname_pass = body.username is not None or body.password is not None
        if body.username is not None:
            if len(body.username) > UNAME_MAX:
                raise HTTPException(status_code=400)
            else:
                user.username = body.username
        if body.password is not None:
            user.password_hash = hash_password(body.password)
        if body.bot_assist is not None:
            user.bot_assist = body.bot_assist
        if updated_uname_pass:
            user.session_id = new_id_str()
            user.session_ttl = new_session_ttl()
        db.add(user)
        db.commit()
        if updated_uname_pass:
            _set_session_cookie(response, user.session_id, user.session_ttl)


# Delete user account
@app.delete("/api/user", status_code=204)
def delete_user(response: Response, session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        if user.is_anon:
            return
        user.is_deleted = True
        user.session_ttl = datetime.utcnow()
        db.add(user)
        db.commit()
        _clear_session_cookie(response)


# Get current user info
@app.get("/api/user", response_model=UserProfileResponse)
def get_current_user(session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        return {
            "username": get_uuname_safe(user),
            "user_id": user.user_id,
            "bot_assist": user.bot_assist,
            "is_anon": user.is_anon,
        }


# Get existing user info
@app.get("/api/user/{user_id}", response_model=UserProfileResponse)
def get_user(user_id: str):
    with Session(engine) as db:
        user = db.get(User, user_id)
        if user is None or user.is_deleted:
            raise HTTPException(status_code=404)
        return {
            "username": get_uuname_safe(user),
            "user_id": user.user_id,
            "bot_assist": user.bot_assist,
            "is_anon": user.is_anon,
        }


# Get user games
@app.get("/api/user/{user_id}/games", response_model=GameListResponse)
def get_user_games(user_id: str):
    with Session(engine) as db:
        # We need to check that the user is not deleted. So we can also
        # check here that the user actually exists, and save a useless db query.
        user = db.get(User, user_id)
        if user is None or user.is_deleted:
            raise HTTPException(status_code=404)

        Player1 = aliased(User)
        Player2 = aliased(User)

        # This query gets all of the fields we're interested in one shot via
        # joining on the User and Move tables
        games = db.exec(
            select(
                Game.game_id,
                Game.is_complete,
                Game.player_id_1,
                Game.player_id_2,
                Game.winner_id,
                Player1.username.label("p1_uname"),
                Player1.is_deleted.label("p1_deleted"),
                Player1.is_anon.label("p1_anon"),
                Player2.username.label("p2_uname"),
                Player2.is_deleted.label("p2_deleted"),
                Player2.is_anon.label("p2_anon"),
                func.count(Move.move_index).label("total_moves"),
            )
            .where(
                (Game.player_id_1 == user_id) | (Game.player_id_2 == user_id),
            ).where(not_(and_(*prune_delete_game_conds(user_id=user_id))))
            .outerjoin(Player1, Game.player_id_1 == Player1.user_id)
            .outerjoin(Player2, Game.player_id_2 == Player2.user_id)
            .outerjoin(Move, Move.game_id == Game.game_id)
            .group_by(Game.game_id)
            .order_by(Game.move_time.desc())
            .limit(200)
        ).all()

        return {"games": [{
            "game_id": game.game_id,
            "is_complete": game.is_complete,
            "is_started": game.player_id_1 is not None and game.player_id_2 is not None,
            "p1_uid": game.player_id_1,
            "p2_uid": game.player_id_2,
            "p1_uname": get_uname_safe(game.p1_uname, game.p1_deleted, game.p1_anon),
            "p2_uname": get_uname_safe(game.p2_uname, game.p2_deleted, game.p2_anon),
            "winner_index": winner_index(game.player_id_1, game.player_id_2, game.winner_id),
            "total_moves": game.total_moves,
        } for game in games]}


# User login. This generates a new session id each time, meaning any other
# session will become invalidated
@app.post("/api/user/login", status_code=204)
def login(body: UserInfoBody, response: Response):
    with Session(engine) as db:
        user = db.exec(select(User).where(User.username == body.username)).first()
        if (user is None or user.is_deleted or not verify_password(body.password, user.password_hash)):
            raise HTTPException(status_code=404)

        user.session_id = new_id_str()
        user.session_ttl = new_session_ttl()
        db.add(user)
        db.commit()
        _set_session_cookie(response, user.session_id, user.session_ttl)


# User logout. This simply sets TTL to now so that it is considered expired
@app.post("/api/user/logout", status_code=204)
def logout(response: Response, session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        user.session_ttl = datetime.utcnow()
        db.add(user)
        db.commit()
        _clear_session_cookie(response)


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
# game id
@app.post("/api/game/join", response_model=str)
def join_game(session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        # Check if user is already in an in-progress game, join the most recent
        # one if so
        existing = db.exec(
            select(Game).where(
                Game.is_complete == False,
                (Game.player_id_1 == user.user_id) | (Game.player_id_2 == user.user_id),
                Game.is_public == True,
            ).where(not_(and_(
                *prune_delete_game_conds(),
            ))).where(not_(and_(
                *prune_finish_game_conds(),
            )))
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
            ).where(not_(and_(
                *prune_delete_game_conds(),
            ))).where(not_(and_(
                *prune_finish_game_conds(),
            )))
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
def join_specific_game(game_id: str, session_id: str = Cookie(...)):
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
def create_private_game(session_id: str = Cookie(...)):
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


# Create a game against the bot
@app.post("/api/game/bot", response_model=str)
def create_bot_game(session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        game = Game(game_id=new_id_str(), is_public=False)
        if random.random() < 0.5:
            game.player_id_1 = user.user_id
            game.player_id_2 = BOT_UID
        else:
            game.player_id_1 = BOT_UID
            game.player_id_2 = user.user_id
        db.add(game)

        # If bot is player 1, it opens with (0, 0, 0)
        if game.player_id_1 == BOT_UID:
            db.add(Move(game_id=game.game_id, move_index=0, a=False, r=0, c=0))

        db.commit()
        return game.game_id


# Get game state. If this user is in this game, and the opponent is AFK, we forfeit
# them before returning the game info.
@app.get("/api/game/{game_id}", response_model=GameStateResponse)
def get_game(game_id: str, session_id: str = Cookie(None)):
    with Session(engine) as db:
        user_id = None
        if session_id is not None:
            user = require_valid_session(session_id, db)
            user_id = user.user_id

        game = get_game_with_unames(game_id, db)
        if game is None:
            raise HTTPException(status_code=404)

        if user_id is not None and not game.is_complete:
            other_last_req = None
            if game.player_id_1 == user_id:
                other_last_req = game.last_req_p2
            elif game.player_id_2 == user_id:
                other_last_req = game.last_req_p1

            min_req = get_min_req()
            if other_last_req is not None and other_last_req < min_req:
                db.exec(
                    update(Game).
                    where(Game.game_id == game_id)
                    .values(
                        is_complete = True,
                        winner_id = user_id,
                    )
                )
                db.commit()
                game = SimpleNamespace(**{**game._asdict(), "is_complete": True, "winner_id": user_id})
            elif game.player_id_1 == user_id or game.player_id_2 == user_id:
                update_game_last_req(game_id, user_id, db)

        moves = db.exec(
            select(Move).where(Move.game_id == game_id).order_by(Move.move_index)
        ).all()
        return build_game_state(game, moves, user_id)


@app.get("/api/game/{game_id}/bot", response_model=BotMoveResponse)
async def get_bot_move(game_id: str, num_moves: int = Query(...), session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        if not user.bot_assist:
            raise HTTPException(status_code=403)
        game = db.get(Game, game_id)
        if game is None:
            raise HTTPException(status_code=404)
        if game.is_complete and num_moves == 0:
            raise HTTPException(status_code=400)

        cached = db.get(BestMove, (game_id, num_moves))
        if cached is not None:
            return {"a": cached.a, "r": cached.r, "c": cached.c}

        move = await _bot.evaluate_ahead(game_id, num_moves)
        if move is None:
            raise HTTPException(status_code=400)

        db.add(BestMove(
            game_id=game_id, move_index=num_moves,
            a=move["a"], r=move["r"], c=move["c"],
        ))
        db.commit()
        return move


# Submit a move. Update this player's last req time
@app.post("/api/game/{game_id}", response_model=Union[GameStateResponse, MoveFailureResponse])
def make_move(game_id: str, body: MoveBody, session_id: str = Cookie(...)):
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

        # Ideally we'd call the same update_game_last_req function, but the
        # opportunity here to batch the updates together seems worth taking
        game.move_time = datetime.utcnow()
        if user.user_id == game.player_id_1:
            game.last_req_p1 = datetime.utcnow()
        elif user.user_id == game.player_id_2:
            game.last_req_p2 = datetime.utcnow()

        db.add(game)
        db.commit()

        # If this is a bot game and the next turn is the bot's, queue evaluation
        if not game.is_complete and (game.player_id_1 == BOT_UID or game.player_id_2 == BOT_UID):
            next_player = get_current_player_id(game, len(moves))
            if next_player == BOT_UID:
                _bot.evaluate_ahead(game_id, 0)

        game = get_game_with_unames(game_id, db)
        return build_game_state(game, moves, user.user_id)


# Undo the most recent move. Update this player's last req time
@app.post("/api/game/undo/{game_id}", response_model=GameStateResponse)
def undo_move(game_id: str, session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)

        game = get_game_with_unames(game_id, db)
        if game is None:
            raise HTTPException(status_code=404)

        if game.is_complete:
            raise HTTPException(status_code=400)

        moves = db.exec(
            select(Move).where(Move.game_id == game_id).order_by(Move.move_index)
        ).all()

        current_player = get_current_player_id(game, len(moves))
        if len(moves) % 2 != 0 or user.user_id != current_player:
            raise HTTPException(status_code=400)

        cached = db.get(BestMove, (game_id, len(moves)))
        if cached is not None:
            db.delete(cached)
        db.delete(moves[-1])
        db.commit()
        update_game_last_req(game_id, user.user_id, db)
        return build_game_state(game, moves[:-1], user.user_id)


# Get a list of recent active games.
# Returns up to 25 games where the game has at least started (both player ids are
# present), and the game is public
@app.post("/api/games", response_model=GameListResponse)
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
                Player1.username.label("p1_uname"),
                Player1.is_deleted.label("p1_deleted"),
                Player1.is_anon.label("p1_anon"),
                Player2.username.label("p2_uname"),
                Player2.is_deleted.label("p2_deleted"),
                Player2.is_anon.label("p2_anon"),
                func.count(Move.move_index).label("total_moves"),
            ).where(
                Game.player_id_1 != None,
                Game.player_id_2 != None,
                Game.is_public == True
            ).where(not_(and_(*prune_delete_game_conds())))
            .outerjoin(Player1, Game.player_id_1 == Player1.user_id)
            .outerjoin(Player2, Game.player_id_2 == Player2.user_id)
            .outerjoin(Move, Move.game_id == Game.game_id)
            .group_by(Game.game_id)
            .order_by(Game.move_time.desc())
            .limit(25)
        ).all()

        return {"games": [{
            "game_id": game.game_id,
            "is_complete": game.is_complete,
            "is_started": game.player_id_1 is not None and game.player_id_2 is not None,
            "p1_uid": game.player_id_1,
            "p2_uid": game.player_id_2,
            "p1_uname": get_uname_safe(game.p1_uname, game.p1_deleted, game.p1_anon),
            "p2_uname": get_uname_safe(game.p2_uname, game.p2_deleted, game.p2_anon),
            "total_moves": game.total_moves,
            "winner_index": winner_index(game.player_id_1, game.player_id_2, game.winner_id)
        } for game in games]}


# Forfeit a game
@app.post("/api/game/forfeit/{game_id}", status_code=204)
def forfeit_game(game_id: str, session_id: str = Cookie(...)):
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


def _get_chat_messages(game_id: str, db: Session, start_index: Optional[int]):
    stmt = (select(ChatMessage, User.username, User.is_deleted, User.is_anon)
        .join(User, ChatMessage.sender_id == User.user_id)
        .where(ChatMessage.game_id == game_id)
        .order_by(ChatMessage.sent_time.desc())
        .limit(MAX_MESSAGES))
    if start_index is not None:
        stmt = stmt.where(ChatMessage.chat_id <= start_index)
    msgs = db.exec(stmt).all()
    return {"messages": [{
        "sender_uname": get_uname_safe(user_name, is_deleted, is_anon),
        "sent_time": msg.sent_time,
        "message": msg.message,
    } for msg, user_name, is_deleted, is_anon in msgs]}


def _require_player_in_game(game: Game, user_id: str):
    if game.player_id_1 != user_id and game.player_id_2 != user_id:
        raise HTTPException(status_code=400)


@app.get("/api/game/{game_id}/chat", response_model=ChatResponse)
def get_chat(game_id: str, index: Optional[int] = Query(None), session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        game = db.get(Game, game_id)
        if game is None:
            raise HTTPException(status_code=404)
        _require_player_in_game(game, user.user_id)

        if index is not None and index < 0:
                raise HTTPException(status_code=400)

        return _get_chat_messages(game_id, db, index)


@app.post("/api/game/{game_id}/chat", response_model=ChatResponse)
def post_chat(game_id: str, body: ChatMessageBody, session_id: str = Cookie(...)):
    with Session(engine) as db:
        user = require_valid_session(session_id, db)
        game = db.get(Game, game_id)
        if game is None:
            raise HTTPException(status_code=404)
        _require_player_in_game(game, user.user_id)

        max_id = db.exec(
            select(func.max(ChatMessage.chat_id)).where(ChatMessage.game_id == game_id)
        ).one()
        next_id = (max_id or 0) + 1
        msg = ChatMessage(
            game_id=game_id,
            chat_id=next_id,
            message=body.message[:CHATMESSAGE_MAX],
            sender_id=user.user_id,
            sent_time=body.sent_time
        )
        db.add(msg)
        db.commit()

        return _get_chat_messages(game_id, db, None)
