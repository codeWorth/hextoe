#!/usr/bin/env python3
"""Parse move map entries from GDB output, convert keys to ARC values, sort by score."""

import re
import sys

MASK_31 = (1 << 31) - 1

def sign_extend_31(val):
    """Sign-extend a 31-bit value to a Python int."""
    val &= MASK_31
    if val & (1 << 30):
        return val - (1 << 31)
    return val

def pos_from_mm_key(key):
    c = sign_extend_31(key & MASK_31)
    key >>= 31
    r = sign_extend_31(key & MASK_31)
    key >>= 31
    a = key & 1
    return a, r, c

def parse_and_sort(text):
    entries = re.findall(r'mme_key\s*=\s*(\d+),\s*mme_value\s*=\s*(-?\d+)', text)
    results = []
    for key_str, val_str in entries:
        key = int(key_str)
        score = int(val_str)
        if key == 0 and score == 0:
            continue
        a, r, c = pos_from_mm_key(key)
        results.append((a, r, c, score))
    results.sort(key=lambda x: x[3], reverse=True)
    for a, r, c, score in results:
        print(f"a={a:2d}  r={r:4d}  c={c:4d}  score={score}")

if __name__ == "__main__":
    text = sys.stdin.read()
    parse_and_sort(text)
