#!/usr/bin/env python3
"""Diagnostics: copy a few generated guest functions (the ones the main thread spins in) into loop_functions.c so
they can be inspected. Uses the project's own generated-code reader. Run from the repo root after Recompile.ps1."""
import re, sys
from pathlib import Path

WANT = [0x1796B0, 0x10CBC0, 0x10D040, 0x10D130, 0x187BA0,
        0x1F2A20, 0x1F5BB0, 0x2193A0, 0x205D20,
        0x1F78D0, 0x1F72B0, 0x2181C0, 0x222A10,
        0x209100, 0x209200, 0x209810, 0x2092A0, 0x209760]
root = Path("ports/mercenaries/src/recomp/gen")
out = []
found = {}
try:
    sys.path.insert(0, "tools/recomp")
    from audit_generated_function_parity import load_functions
    funcs = load_functions(root)
    for a in WANT:
        f = funcs.get(a)
        if f:
            found[a] = (f.source, f.start_line, f.body)
except Exception as e:                      # fall back to a simple scan
    out.append("/* project reader failed: %r; using fallback scan */\n" % (e,))
    rx = re.compile(r"(?m)^void\s+sub_([0-9A-Fa-f]{8})\s*\(void\)\s*\n\{")
    for p in sorted(root.rglob("recomp_[0-9][0-9][0-9][0-9].c")):
        t = p.read_text(encoding="utf-8", errors="replace")
        for m in rx.finditer(t):
            a = int(m.group(1), 16)
            if a in WANT and a not in found:
                end = t.find("\n}\n", m.end())
                found[a] = (p.name, t.count("\n", 0, m.start()) + 1, t[m.start(): end + 3 if end > 0 else m.start() + 60000])
for a in WANT:
    if a in found:
        src, line, body = found[a]
        out.append("/* ===== sub_%08X  (%s:%s) ===== */\n%s\n\n" % (a, src, line, body[:80000]))
    else:
        out.append("/* ===== sub_%08X NOT FOUND ===== */\n\n" % a)
Path("loop_functions.c").write_text("".join(out), encoding="utf-8")
print("wrote loop_functions.c with", len(found), "of", len(WANT), "functions,", sum(len(x) for x in out), "chars")
