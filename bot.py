import ctypes
import os

_dir = os.path.dirname(os.path.abspath(__file__))
_lib = ctypes.cdll.LoadLibrary(os.path.join(_dir, "libbot.so"))

_lib.evaluateAhead.argtypes = [
    ctypes.POINTER(ctypes.c_int),  # as
    ctypes.POINTER(ctypes.c_int),  # rs
    ctypes.POINTER(ctypes.c_int),  # cs
    ctypes.POINTER(ctypes.c_int),  # isP1s
    ctypes.c_int,                  # numMoves
    ctypes.POINTER(ctypes.c_int),  # bestA
    ctypes.POINTER(ctypes.c_int),  # bestR
    ctypes.POINTER(ctypes.c_int),  # bestC
]
_lib.evaluateAhead.restype = ctypes.c_bool


def evaluate_ahead(moves):
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

    ok = _lib.evaluateAhead(a_arr, r_arr, c_arr, p1_arr, n,
                            ctypes.byref(best_a),
                            ctypes.byref(best_r),
                            ctypes.byref(best_c))
    if not ok:
        return None

    return {"a": best_a.value, "r": best_r.value, "c": best_c.value}
