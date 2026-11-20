#!/usr/bin/env python3
"""Add a [perf-mmio] line to the 10-second perf log: how many guest GPU-register accesses
(each one is a SIGSEGV round trip on Android) happen per interval.

    python3 add_mmio_counters.py          # run from the repo root

Edits (idempotent, all-or-nothing):
  src/nv2a/nv2a_mmio_hook.c                                  + nv2a_mmio_get_counts()
  ports/mercenaries-android/app/src/main/cpp/d3d11_gles/d3d11_gles_a.inc   + [perf-mmio] trace in perf_report()
"""
import sys, pathlib
HOOK = pathlib.Path("src/nv2a/nv2a_mmio_hook.c")
INC  = pathlib.Path("ports/mercenaries-android/app/src/main/cpp/d3d11_gles/d3d11_gles_a.inc")

EDITS = {
 HOOK: [(
  "volatile uint32_t g_nv2a_mmio_put_last_value = 0;\n",
  "volatile uint32_t g_nv2a_mmio_put_last_value = 0;\n\n"
  "/* Android perf telemetry: every counted access cost one SIGSEGV round trip. */\n"
  "void nv2a_mmio_get_counts(unsigned long long *reads, unsigned long long *writes, unsigned long long *puts)\n"
  "{\n"
  "    if (reads)  *reads  = (unsigned long long)(unsigned)g_mmio_read_count;\n"
  "    if (writes) *writes = (unsigned long long)(unsigned)g_mmio_write_count;\n"
  "    if (puts)   *puts   = (unsigned long long)g_nv2a_mmio_put_hits;\n"
  "}\n")],
 INC: [(
  "static void perf_report(void)\n{\n    double now = perf_now();\n",
  "void nv2a_mmio_get_counts(unsigned long long *reads, unsigned long long *writes, unsigned long long *puts);\n"
  "static unsigned long long perf_mmio_last[3];\n"
  "static void perf_report(void)\n{\n    double now = perf_now();\n"),
  ("#undef AVG\n    perf_start = now;",
   "    { unsigned long long r = 0, w = 0, p = 0; nv2a_mmio_get_counts(&r, &w, &p);\n"
   "      android_trace(\"[perf-mmio] sigsegv_reads=%llu sigsegv_writes=%llu put_kicks=%llu\", r - perf_mmio_last[0], w - perf_mmio_last[1], p - perf_mmio_last[2]);\n"
   "      perf_mmio_last[0] = r; perf_mmio_last[1] = w; perf_mmio_last[2] = p; }\n"
   "#undef AVG\n    perf_start = now;")],
}
MARK = "nv2a_mmio_get_counts"

def load(p):
    t = p.read_bytes().decode("utf-8"); return t.replace("\r\n", "\n"), "\r\n" in t
plans = []
for path, edits in EDITS.items():
    if not path.is_file(): sys.exit(f"not found: {path} (run from the repo root)")
    text, crlf = load(path)
    if MARK in text: print(f"{path}: already applied"); continue
    for old, _ in edits:
        if text.count(old) != 1: sys.exit(f"{path}: anchor not found exactly once; nothing written")
    for old, new in edits: text = text.replace(old, new, 1)
    plans.append((path, text, crlf))
for path, text, crlf in plans:
    path.write_bytes((text.replace("\n", "\r\n") if crlf else text).encode("utf-8")); print(f"patched {path}")
