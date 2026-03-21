import secrets
from datetime import datetime, timedelta
from typing import Optional

import bcrypt
from fastapi import HTTPException
from sqlmodel import Session, select
from sqlalchemy import func
from sqlalchemy.orm import aliased

from db_schemas import Game, Move, User, ID_LEN

SESSION_TTL_HOURS = 48
MOVE_ALREADY_TAKEN = "ALREADY_TAKEN"
MOVE_TOO_FAR = "TOO_FAR"
MAX_MOVE_DIST = 7

def hash_password(password: str) -> bytes:
    return bcrypt.hashpw(password.encode(), bcrypt.gensalt())


def verify_password(password: str, password_hash: bytes) -> bool:
    return bcrypt.checkpw(password.encode(), password_hash)


def new_id_str() -> str:
    return secrets.token_urlsafe(ID_LEN)


def new_session_ttl() -> datetime:
    return datetime.utcnow() + timedelta(hours=SESSION_TTL_HOURS)


# Return the User for the given session_id, or raise 403 if the ID is invalid
# (could be not present or expired)
def require_valid_session(session_id: str, db: Session) -> User:
    user = db.exec(select(User).where(User.session_id == session_id)).first()
    if user is None or user.is_deleted or user.session_ttl <= datetime.utcnow():
        raise HTTPException(status_code=403)
    return user


def get_uname_safe(uname: str, is_deleted: bool, is_anon: bool) -> str:
    if uname is None:
        return None

    if is_deleted:
        return None
    if is_anon:
        return "Anonymous"
    return uname


def get_uuname_safe(user: User) -> str:
    return get_uname_safe(user.username, user.is_deleted, user.is_anon)


def other_value(a, b, v):
    if v == a:
        return b
    if v == b:
        return a

    raise HTTPException(status_code=500)

# Check if there is a connected line of 6 down and to the right.
#   <0, r, c> -> <1, r, c> moves us down and to the right
#   <1, r, c> -> <0, r+1, c+1> moves us down and to the right
#
#   <a, r, c> -> <a, r, c+1> moves us to the right
#
#   <0, r, c> -> <1, r, c-1> moves us down and to the left
#   <1, r, c> -> <0, r-1, c> moves us down and to the left
def is_line_dr(moves_tbl, pos) -> bool:
    ca, cr, cc = pos
    count = 1
    is_p1 = moves_tbl.get(pos)
    if is_p1 is None:
        return False

    while count < 6:
        cr += (ca & 1)
        cc += (ca & 1)
        ca = 1 - ca
        cv = moves_tbl.get((ca, cr, cc))
        if cv is None or cv != is_p1:
            return False

        count += 1

    return True

# Check if there is a connected line of 6 to the right.
#   <a, r, c> -> <a, r, c+1> moves us to the right
def is_line_r(moves_tbl, pos) -> bool:
    ca, cr, cc = pos
    count = 1
    is_p1 = moves_tbl.get(pos)
    if is_p1 is None:
        return False

    while count < 6:
        cc += 1
        cv = moves_tbl.get((ca, cr, cc))
        if cv is None or cv != is_p1:
            return False

        count += 1

    return True

# Check if there is a connected line of 6 down and to the left.
#   <0, r, c> -> <1, r, c-1> moves us down and to the left
#   <1, r, c> -> <0, r+1, c> moves us down and to the left
def is_line_dl(moves_tbl, pos) -> bool:
    ca, cr, cc = pos
    count = 1
    is_p1 = moves_tbl.get(pos)
    if is_p1 is None:
        return False

    while count < 6:
        cr += (ca & 1)
        ca = 1 - ca
        cc -= (ca & 1)
        cv = moves_tbl.get((ca, cr, cc))
        if cv is None or cv != is_p1:
            return False

        count += 1

    return True

# Return 0 if no one has won yet, 1 if player 1 got 6 in a row, or 2 if player
# 2 got 6 in a row. If both players have somehow gotten 6 in a row, the result
# is nondeterministic
def check_winner(moves: list[Move]) -> int:
    mvt = {(int(m.a), m.r, m.c): is_player_one(m.move_index) for m in moves}
    for pos in mvt.keys():
        if (is_line_r(mvt, pos) or is_line_dr(mvt, pos) or is_line_dl(mvt, pos)):
            if mvt.get(pos):
                return 1
            else:
                return 2

    return 0


# Return if this number of moves indicates player one should take a turn. If false,
# player two should take a turn
def is_player_one(num_moves: int) -> bool:
    if num_moves == 0:
        return True
    if ((num_moves - 1) // 2) % 2 == 0:
        return False
    return True


# Return the user_id of the player whose turn it is
def get_current_player_id(game: Game, num_moves: int) -> Optional[str]:
    if game.player_id_1 is None or game.player_id_2 is None:
        return None
    if is_player_one(num_moves):
        return game.player_id_1
    else:
        return game.player_id_2


# Construct the game state dict for sending to the client.
# Pass None for asker_id if it's not a logged in request
def build_game_state(game: Game, moves: list[Move], asker_id: str) -> dict:
    is_your_turn = False
    if not game.is_complete and asker_id is not None:
        is_your_turn = asker_id == get_current_player_id(game, len(moves))
    return {
        "player_id_1": game.player_id_1,
        "player_id_2": game.player_id_2,
        "p1_uname": get_uname_safe(game.p1_uname, game.p1_deleted, game.p1_anon),
        "p2_uname": get_uname_safe(game.p2_uname, game.p2_deleted, game.p2_anon),
        "winner_id": game.winner_id,
        "is_your_turn": is_your_turn,
        "moves": [{
                    "a": int(m.a),
                    "r": m.r,
                    "c": m.c,
                    "p1": is_player_one(m.move_index)
                  } for m in moves],
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


# Get the winner index, 0 if neither, 1 if p1, 2 if p2
def winner_index(pid1, pid2, winner_id):
    if pid1 is None or pid2 is None or winner_id is None:
        return 0

    if winner_id == pid1:
        return 1
    if winner_id == pid2:
        return 2

    return 0


# Get game with usernames
def get_game_with_unames(game_id, db):
    Player1 = aliased(User)
    Player2 = aliased(User)
    return db.exec(
        select(
            Game.game_id,
            Game.player_id_1,
            Game.player_id_2,
            Game.winner_id,
            Game.is_complete,
            Game.creation_time,
            Game.move_time,
            Game.is_public,
            func.any_value(Player1.username).label("p1_uname"),
            func.any_value(Player1.is_deleted).label("p1_deleted"),
            func.any_value(Player1.is_anon).label("p1_anon"),
            func.any_value(Player2.username).label("p2_uname"),
            func.any_value(Player2.is_deleted).label("p2_deleted"),
            func.any_value(Player2.is_anon).label("p2_anon"),
        )
        .where(Game.game_id == game_id)
        .outerjoin(Player1, Game.player_id_1 == Player1.user_id)
        .outerjoin(Player2, Game.player_id_2 == Player2.user_id)
    ).first()
