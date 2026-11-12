
#!/usr/bin/env python3
"""Android diagnostics #9 (idempotent): remember the names of the last 32 GL/EGL calls made by the GLES layer so the
crash report can list them. Every call site gl*(...) becomes (crumb("gl*"), gl*(...)) via function-like macros."""
import re
base = "ports/mercenaries-android/app/src/main/cpp/"
inc = [base + "d3d11_gles/d3d11_gles_%s.inc" % c for c in "abc"]
src = "".join(open(p, encoding="utf-8", errors="replace").read() for p in inc)
names = sorted(set(re.findall(r"\b(gl[A-Z][A-Za-z0-9]*)\s*\(", src)))

def macros(ns):
    return "".join("#define %s(...) (gl_crumb(\"%s\"), %s(__VA_ARGS__))\n" % (n, n, n) for n in ns)

HEAD = """
/* GL_CRUMBS: ring of the last GL calls, printed by the crash report */
volatile const char *g_gl_crumbs[32];
volatile unsigned g_gl_crumb_pos;
static inline void gl_crumb(const char *n) { g_gl_crumbs[g_gl_crumb_pos++ & 31] = n; }
"""
a = inc[0]
s = open(a, encoding="utf-8").read()
if "GL_CRUMBS" in s:
    print("gles layer: already applied")
else:
    key = '#include "gles_gl.h"\n'
    if key not in s:
        print("WARNING: gles_gl.h include not found")
    else:
        s = s.replace(key, key + HEAD + macros(names), 1)
        open(a, "w", encoding="utf-8").write(s)
        print("gles layer: applied,", len(names), "functions")

g = base + "android/android_gl.c"
s = open(g, encoding="utf-8").read()
if "GL_CRUMBS" in s:
    print("android_gl.c: already applied")
else:
    egl = sorted(set(re.findall(r"\b(egl[A-Z][A-Za-z0-9]*)\s*\(", s)))
    key = "#include <GLES3/gl3.h>\n"
    if key not in s:
        print("WARNING: android_gl.c include not found")
    else:
        blk = "\n/* GL_CRUMBS */\nextern volatile const char *g_gl_crumbs[32];\nextern volatile unsigned g_gl_crumb_pos;\n" \
              "static inline void gl_crumb(const char *n) { g_gl_crumbs[g_gl_crumb_pos++ & 31] = n; }\n" + macros(egl)
        open(g, "w", encoding="utf-8").write(s.replace(key, key + blk, 1))
        print("android_gl.c: applied,", len(egl), "functions")