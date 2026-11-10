#!/usr/bin/env python3
"""Android fixes #2 (idempotent, run from the repo root):
 A) host threads get at least a 16 MB stack (guest code asks for Xbox-sized stacks; host renderer code needs more)
 B) the opt-in gameplay-draw debug capture is skipped on Android (huge stack frame on every draw)
 C) crash.log also records sp / stack range of the crashing thread."""
import sys

def patch(path, old, new, tag):
    s = open(path, encoding="utf-8").read()
    if tag in s:
        print(tag, "already applied"); return
    if old not in s:
        print("WARNING pattern not found for", tag, "in", path); return
    s = s.replace(old, new, 1)
    open(path, "w", encoding="utf-8").write(s)
    print("applied", tag)

# A
patch("src/platform/win32_compat.c",
      "pthread_attr_setstacksize(&attr, stackSize < 65536 ? 65536 : stackSize);",
      "pthread_attr_setstacksize(&attr, stackSize < (16u << 20) /*MIN_HOST_STACK*/ ? (16u << 20) : stackSize);",
      "MIN_HOST_STACK")

# B
HDR = """static void debug_capture_gameplay_draw_before(const char *kind,
                                               uint32_t vertex_count,
                                               uint32_t vertex_base,
                                               const void *guest_vertices,
                                               uint32_t guest_stride,
                                               int combiner_active)
{"""
NEW = """static void debug_capture_gameplay_draw_before_impl(const char *kind, uint32_t vertex_count, uint32_t vertex_base,
                                                    const void *guest_vertices, uint32_t guest_stride,
                                                    int combiner_active) __attribute__((noinline, unused));
static void debug_capture_gameplay_draw_before(const char *kind,
                                               uint32_t vertex_count,
                                               uint32_t vertex_base,
                                               const void *guest_vertices,
                                               uint32_t guest_stride,
                                               int combiner_active)
{
    /* ANDROID_SKIP_GAMEPLAY_CAPTURE */
    g_debug_gameplay_draw_after_pending = 0;
    (void)kind; (void)vertex_count; (void)vertex_base; (void)guest_vertices; (void)guest_stride; (void)combiner_active;
}
static void debug_capture_gameplay_draw_before_impl(const char *kind,
                                                    uint32_t vertex_count,
                                                    uint32_t vertex_base,
                                                    const void *guest_vertices,
                                                    uint32_t guest_stride,
                                                    int combiner_active)
{"""
patch("src/nv2a/nv2a_pgraph_d3d11.c", HDR, NEW, "ANDROID_SKIP_GAMEPLAY_CAPTURE")

# C
F = "ports/mercenaries-android/app/src/main/cpp/android/android_fault.c"
patch(F, "#include <dlfcn.h>\n", "#include <dlfcn.h>\n#include <pthread.h>\n", "#include <pthread.h>")
patch(F, "(unsigned)g_recomp_current_func);\n    cw(fd, b);\n",
      """(unsigned)g_recomp_current_func);
    cw(fd, b);
#if defined(__aarch64__)
    { /* STACKINFO */
        ucontext_t *u2 = (ucontext_t *)ucv; pthread_attr_t pa; void *lo = NULL; size_t sz = 0;
        if (pthread_getattr_np(pthread_self(), &pa) == 0) { pthread_attr_getstack(&pa, &lo, &sz); pthread_attr_destroy(&pa); }
        snprintf(b, sizeof(b), "sp %p stack_lo %p stack_size %zu\\n", (void *)u2->uc_mcontext.sp, lo, sz);
        cw(fd, b);
    }
#endif
""", "STACKINFO")
