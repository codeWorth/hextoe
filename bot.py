import ctypes
import os
import asyncio
import time
import threading
from types import SimpleNamespace

from datetime import datetime

from sqlmodel import Session, select

from db_schemas import Game, Move
from util import is_player_one, check_winner, get_current_player_id, BOT_UID

tasks = {}
_lock = threading.Lock()
_loop = None

_dir = os.path.dirname(os.path.abspath(__file__))
_lib = ctypes.cdll.LoadLibrary(os.path.join(_dir, "libbot.so"))

_lib.evaluate_ahead.argtypes = [
    ctypes.POINTER(ctypes.c_int),  # as
    ctypes.POINTER(ctypes.c_int),  # rs
    ctypes.POINTER(ctypes.c_int),  # cs
    ctypes.POINTER(ctypes.c_int),  # is_p1s
    ctypes.c_int,                  # num_moves
    ctypes.POINTER(ctypes.c_int),  # best_a
    ctypes.POINTER(ctypes.c_int),  # best_r
    ctypes.POINTER(ctypes.c_int),  # best_c
]
_lib.evaluate_ahead.restype = ctypes.c_bool


def proc_eval_task(engine):
    with _lock:
        if len(tasks) == 0:
            return
        task_key = min(tasks.items(), key=lambda item: item[1].creation_time)[0]
        game_id = task_key[0]
        num_moves = task_key[1]
        eval_task = tasks.pop(task_key)

    with Session(engine) as db:
        game = db.get(Game, game_id)
        is_bot = game is not None and (game.player_id_1 == BOT_UID or game.player_id_2 == BOT_UID)

        moves = db.exec(
            select(Move).where(Move.game_id == game_id).order_by(Move.move_index)
        ).all()
        moves_list = [
            {"a": int(m.a), "r": m.r, "c": m.c, "p1": is_player_one(m.move_index)}
            for m in moves
        ]
        if num_moves > 0:
            moves_list = moves_list[:num_moves]

        move = do_evaluate_ahead(moves_list)

        # For bot games with num_moves==0, add moves to the DB
        if is_bot and num_moves == 0 and move is not None and not game.is_complete:
            current_player = get_current_player_id(game, len(moves))
            while current_player == BOT_UID and move is not None and not game.is_complete:
                new_move = Move(
                    game_id=game_id,
                    move_index=len(moves),
                    a=bool(move["a"]),
                    r=move["r"],
                    c=move["c"],
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

                current_player = get_current_player_id(game, len(moves))
                if current_player == BOT_UID and not game.is_complete:
                    moves_list = [
                        {"a": int(m.a), "r": m.r, "c": m.c, "p1": is_player_one(m.move_index)}
                        for m in moves
                    ]
                    move = do_evaluate_ahead(moves_list)

        _loop.call_soon_threadsafe(eval_task.future_eval.set_result, move)


# Queue up an evaluation for the given game_id and num_moves
def evaluate_ahead(game_id, num_moves):
    key = (game_id, num_moves)
    with _lock:
        eval_task = tasks.get(key)
        if eval_task is not None:
            return eval_task.future_eval
        eval_task = SimpleNamespace(
            future_eval=_loop.create_future(),
            creation_time=time.time(),
        )
        tasks[key] = eval_task
        return eval_task.future_eval


def do_evaluate_ahead(moves):
    """Run minimax evaluation on the given moves.

    moves: list of dicts with keys "a", "r", "c", "p1"
    Returns {"a": int, "r": int, "c": int} or None if no move found.
    """
    n = len(moves)
    IntArray = ctypes.c_int * n
    a_arr = IntArray(*(m["a"] for m in moves))
    r_arr = IntArray(*(m["r"] for m in moves))
    c_arr = IntArray(*(m["c"] for m in moves))
    p1_arr = IntArray(*(1 if m["p1"] else 0 for m in moves))

    best_a = ctypes.c_int()
    best_r = ctypes.c_int()
    best_c = ctypes.c_int()

    ok = _lib.evaluate_ahead(a_arr, r_arr, c_arr, p1_arr, n,
                            ctypes.byref(best_a),
                            ctypes.byref(best_r),
                            ctypes.byref(best_c))
    if not ok:
        return None

    return {"a": best_a.value, "r": best_r.value, "c": best_c.value}
