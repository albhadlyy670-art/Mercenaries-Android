/* Fuzzes the project's NV2A vertex-shader and register-combiner HLSL generators, translates the output
 * to GLSL ES, and compiles it with the real GLES driver. */
#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3d8_vsh.h"
#include "d3d8_combiners.h"
#include "gles_gl.h"
#include "hlsl2glsl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void *EGLDisplay,*EGLConfig,*EGLContext,*EGLSurface; typedef int32_t EGLint; typedef unsigned EGLBoolean,EGLenum;
EGLDisplay eglGetPlatformDisplay(EGLenum,void*,const EGLint*); EGLBoolean eglInitialize(EGLDisplay,EGLint*,EGLint*); EGLBoolean eglBindAPI(EGLenum);
EGLBoolean eglChooseConfig(EGLDisplay,const EGLint*,EGLConfig*,EGLint,EGLint*); EGLContext eglCreateContext(EGLDisplay,EGLConfig,EGLContext,const EGLint*);
EGLBoolean eglMakeCurrent(EGLDisplay,EGLSurface,EGLSurface,EGLContext);
static unsigned rng = 12345; static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }
static int glsl_ok(const char *src, int ps, char *log, int n)
{
    GLuint s = glCreateShader(ps ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER); glShaderSource(s, 1, &src, 0); glCompileShader(s); GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) glGetShaderInfoLog(s, n, 0, log); glDeleteShader(s); return ok;
}
typedef struct { uint32_t magic, stage, gs, glsl_len; HlslResult r; } BlobHdr;   /* private layout of d3d11_gles.c shader blobs */
static const char *blob_glsl(ID3D10Blob *b)   /* stored length-prefixed, not NUL-terminated */
{ static char buf[1 << 20]; const BlobHdr *h = (const BlobHdr *)ID3D10Blob_GetBufferPointer(b); memcpy(buf, (const char *)h + sizeof(BlobHdr), h->glsl_len); buf[h->glsl_len] = 0; return buf; }
static char hlsl[1 << 20]; static int glfail_vs, glfail_ps; static char glerr[2048];
int main(int argc, char **argv)
{
    int N = argc > 1 ? atoi(argv[1]) : 300;
    EGLDisplay d = eglGetPlatformDisplay(0x31DD, 0, 0); eglInitialize(d, 0, 0); eglBindAPI(0x30A0);
    EGLint a[] = { 0x3040, 0x40, 0x3033, 1, 0x3038 }; EGLConfig c; EGLint n; eglChooseConfig(d, a, &c, 1, &n);
    EGLint ca[] = { 0x3098, 3, 0x3038 }; EGLContext x = eglCreateContext(d, c, 0, ca); eglMakeCurrent(d, 0, 0, x);
    int gen_fail = 0, xl_fail = 0, gl_fail = 0, ok = 0; char log[2048]; char firsterr[3][600]; int nerr = 0;
    /* --- vertex shaders --- */
    for (int i = 0; i < N; ++i) {
        DWORD mc[136 * 4]; int len = 1 + (int)(rnd() % 12);
        for (int k = 0; k < len * 4; ++k) mc[k] = rnd() ^ (rnd() << 12);
        mc[(len - 1) * 4 + 3] |= 1;                                        /* FINAL bit */
        static NV2AVshProgram prog; memset(&prog, 0, sizeof(prog)); d3d8_vsh_parse(mc, len, &prog);
        int w = d3d8_vsh_generate_hlsl(&prog, hlsl, sizeof(hlsl)); if (w <= 0) { gen_fail++; continue; }
        ID3D10Blob *code = NULL, *err = NULL;
        if (FAILED(D3DCompile(hlsl, (SIZE_T)w, "vsh", NULL, NULL, "main", "vs_5_0", 0, 0, &code, &err))) { xl_fail++; { FILE *ef = fopen("/tmp/errs.txt", "a"); fprintf(ef, "%s\n", (char *)ID3D10Blob_GetBufferPointer(err)); fclose(ef); } if (nerr == 0) { FILE *f = fopen("/tmp/fail.hlsl", "w"); fwrite(hlsl, 1, (size_t)w, f); fclose(f); } if (nerr < 3) snprintf(firsterr[nerr++], 600, "VS translate: %s", (char *)ID3D10Blob_GetBufferPointer(err)); continue; }
        const char *glsl = blob_glsl(code);
        if (!glsl_ok(glsl, 0, glerr, sizeof(glerr))) { glfail_vs++; if (glfail_vs == 1) { FILE *f = fopen("/tmp/fail_vs.glsl", "w"); fputs(glsl, f); fclose(f); FILE *g = fopen("/tmp/fail_vs.hlsl", "w"); fwrite(hlsl, 1, (size_t)w, g); fclose(g); } if (glfail_vs <= 2) printf("  Mesa rejected generated VS: %.300s\n", glerr); }
        ok++;
    }
    printf("vertex shaders: %d generated+translated, %d generator-empty, %d translator errors (of %d)\n", ok, gen_fail, xl_fail, N);
    /* --- combiners --- */
    int cok = 0, cgen = 0, cxl = 0; 
    for (int i = 0; i < N; ++i) {
        static NV2ACombinerState st; memset(&st, 0, sizeof(st)); st.num_stages = 1 + (int)(rnd() % 8);
        unsigned *raw = (unsigned *)&st; size_t words = sizeof(st) / 4;
        for (size_t k = 0; k < words; ++k) raw[k] = rnd() % 7;           /* small enum values */
        st.num_stages = 1 + (int)(rnd() % 8);
        int w = d3d8_combiners_generate_hlsl(&st, hlsl, sizeof(hlsl)); if (w <= 0) { cgen++; continue; }
        ID3D10Blob *code = NULL, *err = NULL;
        if (FAILED(D3DCompile(hlsl, (SIZE_T)w, "ps", NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err))) { cxl++; if (nerr < 3) snprintf(firsterr[nerr++], 600, "PS translate: %s", (char *)ID3D10Blob_GetBufferPointer(err)); continue; }
        { const char *glsl = blob_glsl(code);
          if (!glsl_ok(glsl, 1, glerr, sizeof(glerr))) { glfail_ps++; if (glfail_ps == 1) { FILE *f = fopen("/tmp/fail_ps.glsl", "w"); fputs(glsl, f); fclose(f); FILE *g = fopen("/tmp/fail_ps.hlsl", "w"); fwrite(hlsl, 1, (size_t)w, g); fclose(g); } if (glfail_ps <= 2) printf("  Mesa rejected generated PS: %.300s\n", glerr); } }
        cok++;
    }
    printf("combiner shaders: %d generated+translated, %d generator-empty, %d translator errors (of %d)\n", cok, cgen, cxl, N);
    printf("Mesa GLSL ES compile of translated output: %d/%d vertex, %d/%d combiner shaders rejected\n", glfail_vs, ok, glfail_ps, cok);
    for (int i = 0; i < nerr; ++i) printf("  e.g. %s\n", firsterr[i]);
    return 0;
}
