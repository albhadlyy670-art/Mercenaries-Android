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
patch(F, "#include <dlfcn.h>\n", "#include <dlfcn.h>\n#include <sys/prctl.h>\n#include <sys/syscall.h>\n", "<sys/prctl.h>")
patch(F, "static void dump_state(const char *tag, int flags, int sig, siginfo_t *si, void *ucv)\n{",
      """/* MAPS_HELPER: print the /proc/self/maps line that contains an address */
static void map_line_for(int fd, const char *label, uintptr_t a)
{
    static char mb[1 << 16];
    int mf = open("/proc/self/maps", O_RDONLY);
    if (mf < 0) return;
    char ln[256]; size_t ll = 0; ssize_t r;
    while ((r = read(mf, mb, sizeof(mb))) > 0) {
        for (ssize_t i = 0; i < r; ++i) {
            char c = mb[i];
            if (c == '\\n') {
                ln[ll] = 0;
                char *dash = strchr(ln, '-');
                unsigned long s0 = strtoul(ln, NULL, 16), e0 = dash ? strtoul(dash + 1, NULL, 16) : 0;
                if (a >= s0 && a < e0) { char b2[320]; snprintf(b2, sizeof(b2), "%s %s\\n", label, ln); cw(fd, b2); }
                ll = 0;
            } else if (ll < sizeof(ln) - 1) ln[ll++] = c;
        }
    }
    close(mf);
}

static void dump_state(const char *tag, int flags, int sig, siginfo_t *si, void *ucv)
{""", "MAPS_HELPER")
patch(F, "(unsigned)g_recomp_current_func);\n    cw(fd, b);\n",
      """(unsigned)g_recomp_current_func);
    cw(fd, b);
#if defined(__aarch64__)
    { /* STACKINFO */
        ucontext_t *u2 = (ucontext_t *)ucv; pthread_attr_t pa; void *lo = NULL; size_t sz = 0;
        if (pthread_getattr_np(pthread_self(), &pa) == 0) { pthread_attr_getstack(&pa, &lo, &sz); pthread_attr_destroy(&pa); }
        snprintf(b, sizeof(b), "sp %p stack_lo %p stack_size %zu\\n", (void *)u2->uc_mcontext.sp, lo, sz);
        cw(fd, b);
        { char nm_[24] = {0}; prctl(PR_GET_NAME, nm_);
          snprintf(b, sizeof(b), "tid %d name %s\\n", (int)syscall(SYS_gettid), nm_); cw(fd, b); }
        map_line_for(fd, "map(fault)", si ? (uintptr_t)si->si_addr : 0);
        map_line_for(fd, "map(sp)", (uintptr_t)u2->uc_mcontext.sp);
        map_line_for(fd, "map(pc)", (uintptr_t)u2->uc_mcontext.pc);
    }
#endif
""", "STACKINFO")
