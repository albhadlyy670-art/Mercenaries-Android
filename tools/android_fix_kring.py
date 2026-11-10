#!/usr/bin/env python3
"""Android diagnostics #6 (idempotent): remember the last 64 kernel-call ordinals for the watchdog samples."""
p = "src/kernel/kernel_bridge.c"
s = open(p, encoding="utf-8").read()
if "g_android_kring" in s:
    print("already applied")
else:
    old = "    g_kernel_call_count++;\n"
    if old not in s:
        print("WARNING pattern not found")
    else:
        new = old + """#ifdef __ANDROID__
    { extern volatile unsigned g_android_kring[64]; extern volatile unsigned g_android_kpos;
      g_android_kring[g_android_kpos & 63] = (unsigned)ordinal; ++g_android_kpos; }
#endif
"""
        open(p, "w", encoding="utf-8").write(s.replace(old, new, 1))
        print("applied")
