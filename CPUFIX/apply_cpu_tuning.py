#!/usr/bin/env python3
"""Run from the repo root:  python3 apply_cpu_tuning.py
Copies android_cpu_tuning.inc next to android_game.c and adds the 2 edits (idempotent)."""
import os, shutil, sys
D = "ports/mercenaries-android/app/src/main/cpp/android"
G = os.path.join(D, "android_game.c")
here = os.path.dirname(os.path.abspath(__file__))
if not os.path.exists(G): sys.exit("run from the repo root (android_game.c not found at %s)" % G)
shutil.copyfile(os.path.join(here, "android_cpu_tuning.inc"), os.path.join(D, "android_cpu_tuning.inc"))
s = open(G, encoding="utf-8", newline="").read()
nl = "\r\n" if "\r\n" in s else "\n"
a1 = "extern int mercenaries_android_main(void);" + nl
a2 = '    android_trace("[app] entering mercenaries_android_main");'
if '#include "android_cpu_tuning.inc"' not in s:
    if s.count(a1) != 1: sys.exit("anchor 1 not found exactly once")
    s = s.replace(a1, a1 + '#include "android_cpu_tuning.inc"' + nl)
if "android_cpu_tuning_start(" not in s:
    if s.count(a2) != 1: sys.exit("anchor 2 not found exactly once")
    s = s.replace(a2, "    android_cpu_tuning_start(tmp_dir);" + nl + a2)
open(G, "w", encoding="utf-8", newline="").write(s)
print("done: review with  git diff")
