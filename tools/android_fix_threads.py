#!/usr/bin/env python3
"""Android diagnostics #4 (idempotent, run from the repo root): tell the app about every guest thread so the
watchdog can sample all of them and events.log lists thread creation."""
p = "src/platform/win32_compat.c"
s = open(p, encoding="utf-8").read()
if "android_note_thread" in s:
    print("already applied")
else:
    old = "    pthread_attr_destroy(&attr);\n    o->thread_joinable = 1;\n"
    if old not in s:
        print("WARNING pattern not found")
    else:
        new = old + """#ifdef __ANDROID__
    { extern void android_note_thread(pthread_t, const void *, unsigned, size_t);
      android_note_thread(o->thread, (const void *)start, (unsigned)o->tid, (size_t)stackSize); }
#endif
"""
        open(p, "w", encoding="utf-8").write(s.replace(old, new, 1))
        print("applied")
