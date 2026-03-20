import secrets
from datetime import datetime, timedelta
from typing import Optional

from fastapi import HTTPException
from passlib.context import CryptContext
from sqlmodel import Session, select

from db_schemas import Game, Move, User

SESSION_TTL_HOURS = 48
MOVE_ALREADY_TAKEN = "ALREADY_TAKEN"
MOVE_TOO_FAR = "TOO_FAR"
MAX_MOVE_DIST = 7

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")


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
