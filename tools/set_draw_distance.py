#!/usr/bin/env python3
"""Set the Android world draw-distance cap in recomp_options.c.

    python3 set_draw_distance.py            # 150 units (default)
    python3 set_draw_distance.py 120        # any other value
    python3 set_draw_distance.py 150 path/to/recomp_options.c

Changes two Android-only values (the Windows build is untouched):
  recomp_options_world_view_distance()   camera far plane / scenery cap  -> N
  recomp_options_streaming_grid_radius() world streaming grid            -> N + 60
Re-running with a different N just updates the numbers. Writes nothing if an anchor is missing.
"""
import re, sys, pathlib

DEFAULT = "ports/mercenaries/src/recomp_options.c"
n = float(sys.argv[1]) if len(sys.argv) > 1 else 150.0
path = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else DEFAULT)
if not path.is_file():
    sys.exit(f"not found: {path} (run from the repo root or pass the path)")
if not 20.0 <= n <= 300.0:
    sys.exit("distance must be between 20 and 300 (300 is the retail value)")

raw = path.read_bytes().decode("utf-8")
crlf = "\r\n" in raw
text = raw.replace("\r\n", "\n")
grid = n + 60.0
fmt = lambda v: f"{v:.0f}.0f" if float(v).is_integer() else f"{v}f"

p1 = re.compile(r"if \(distance > [0-9.]+f\) return [0-9.]+f;")
p2 = re.compile(r"return [0-9.]+f; /\* [0-9]+ units beyond the normal camera/scenery cap\. \*/")
for name, p in (("view-distance cap", p1), ("streaming grid radius", p2)):
    if len(p.findall(text)) != 1:
        sys.exit(f"anchor for {name} not found exactly once - file differs from the inspected revision; nothing written")

text = p1.sub(f"if (distance > {fmt(n)}) return {fmt(n)};", text)
text = p2.sub(f"return {fmt(grid)}; /* 60 units beyond the normal camera/scenery cap. */", text)
text = re.sub(r"The [0-9]+-unit streaming radius exceeds the [0-9]+-unit camera/scenery cap",
              f"The {grid:.0f}-unit streaming radius exceeds the {n:.0f}-unit camera/scenery cap", text)
path.write_bytes((text.replace("\n", "\r\n") if crlf else text).encode("utf-8"))
print(f"{path}: Android view cap = {fmt(n)}, streaming grid = {fmt(grid)}")
