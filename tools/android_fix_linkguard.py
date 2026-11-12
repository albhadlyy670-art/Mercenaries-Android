#!/usr/bin/env python3
"""Android fix #10 (idempotent): the Mali driver crashes inside glLinkProgram for one shader pair. Wrap the call so a
crash there is caught (sigsetjmp/siglongjmp from the SEGV handler), log both shaders' GLSL into crash.log, skip the
pair, and let the game carry on."""
base = "ports/mercenaries-android/app/src/main/cpp/"
b = base + "d3d11_gles/d3d11_gles_b.inc"
s = open(b, encoding="utf-8").read()
if "LINK_GUARD" in s:
    print("gles layer: already applied")
else:
    GUARD = '''/* LINK_GUARD: survive a driver crash inside glLinkProgram and record the shaders involved */
#include <setjmp.h>
#include <fcntl.h>
#include <unistd.h>
void android_trace(const char *fmt, ...);
extern const char *g_android_crash_path;
__thread void *g_android_link_guard;
static char g_glsl_v[5500], g_glsl_f[5500];
static void android_note_glsl(const char *v, const char *f)
{
    snprintf(g_glsl_v, sizeof(g_glsl_v), "%s", v ? v : "(null)");
    snprintf(g_glsl_f, sizeof(g_glsl_f), "%s", f ? f : "(null)");
}
static int android_guarded_link(GLuint prog)
{
    sigjmp_buf jb;
    if (sigsetjmp(jb, 1) == 0) { g_android_link_guard = &jb; glLinkProgram(prog); g_android_link_guard = NULL; return 1; }
    g_android_link_guard = NULL;
    int fd = g_android_crash_path ? open(g_android_crash_path, O_WRONLY | O_CREAT | O_APPEND, 0644) : -1;
    if (fd >= 0) {
        const char *h = "[LINK-CRASH] the GPU driver crashed in glLinkProgram; this shader pair is skipped\\n--- vertex ---\\n";
        ssize_t r = write(fd, h, strlen(h)); r = write(fd, g_glsl_v, strlen(g_glsl_v));
        const char *m = "\\n--- fragment ---\\n"; r = write(fd, m, strlen(m)); r = write(fd, g_glsl_f, strlen(g_glsl_f));
        r = write(fd, "\\n", 1); (void)r; close(fd);
    }
    android_trace("[gl] glLinkProgram crashed in the driver; shader pair skipped");
    return 0;
}

'''
    key = "static Program *get_program(Shader *vs, Shader *ps, uint32_t bgra_mask)\n"
    old1 = "char *vsrc = build_vs_source(vs, ps, bgra_mask);\n"
    old2 = "glAttachShader(p->prog, f); glLinkProgram(p->prog);\n"
    if key not in s or old1 not in s or old2 not in s:
        print("WARNING: patterns not found in d3d11_gles_b.inc")
    else:
        s = s.replace(key, GUARD + key, 1)
        s = s.replace(old1, old1 + "    android_note_glsl(vsrc, ps->glsl);\n", 1)
        s = s.replace(old2, "glAttachShader(p->prog, f);\n    if (!android_guarded_link(p->prog)) { glDeleteShader(v); glDeleteShader(f); glDeleteProgram(p->prog); free(p); return NULL; }\n", 1)
        open(b, "w", encoding="utf-8").write(s)
        print("gles layer: applied")

fpath = base + "android/android_fault.c"
s = open(fpath, encoding="utf-8").read()
if "LINK_GUARD" in s:
    print("android_fault.c: already applied")
else:
    old = "static void segv_handler(int sig, siginfo_t *si, void *ucv)\n{\n"
    if old not in s:
        print("WARNING: segv_handler not found")
    else:
        new = old + """    { /* LINK_GUARD: a crash inside glLinkProgram is recoverable (see android_guarded_link) */
        extern __thread void *g_android_link_guard;
        if (g_android_link_guard && sig == SIGSEGV) {
            void *g_ = g_android_link_guard; g_android_link_guard = NULL;
            siglongjmp(*(sigjmp_buf *)g_, 1);
        }
    }
"""
        s = s.replace(old, new, 1)
        s = s.replace("#include <dlfcn.h>\n", "#include <dlfcn.h>\n#include <setjmp.h>\n", 1) if "<setjmp.h>" not in s else s
        open(fpath, "w", encoding="utf-8").write(s)
        print("android_fault.c: applied")
