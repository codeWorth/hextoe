import re
import sys

text = sys.stdin.read() if not sys.stdin.isatty() else ""

as_, rs, cs, is_p1s = [], [], [], []

for line in text.strip().splitlines():
    m = re.match(r'\d+\.\s+([XO])\s+\((-?\d+),\s*(-?\d+),\s*(-?\d+)\)', line)
    if not m:
        continue
    player, a, r, c = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
    as_.append(a)
    rs.append(r)
    cs.append(c)
    is_p1s.append(1 if player == 'X' else 0)

print(f"\tint as[{len(as_)}] = ", as_)
print(f"\tint rs[{len(as_)}] = ", rs)
print(f"\tint cs[{len(as_)}] = ", cs)
print(f"\tint is_p1s[{len(as_)}] = ", is_p1s)
