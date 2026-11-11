#!/usr/bin/env python3
"""Android diagnostics #7 (idempotent): switch on the project's own asset-loader tracer ([ASSET-FLOW] lines on stderr)."""
p = "ports/mercenaries-android/app/src/main/cpp/android/android_game.c"
s = open(p, encoding="utf-8").read()
if "MERCENARIES_TRACE_ASSET_FLOW" in s:
    print("already applied")
else:
    old = '    setenv("MERCENARIES_SAVE_DIR", save_dir, 1);\n'
    if old not in s:
        print("WARNING pattern not found")
    else:
        new = old + '    setenv("MERCENARIES_TRACE_ASSET_FLOW", "1", 1);      /* diagnostics: [ASSET-FLOW] lines into game.log */\n'
        open(p, "w", encoding="utf-8").write(s.replace(old, new, 1))
        print("applied")
