/*
 * android_gl.c - EGL + OpenGL ES 3.0 surface owned by the game thread.
 *
 * Owns the on-screen EGL surface. The game renders through the D3D11-on-GLES
 * layer (d3d11_gles/) into an offscreen back buffer; android_gl_present_texture
 * blits it here and draws the touch overlay.
 */
#include "android_host.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <string.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "MercRecomp", __VA_ARGS__)

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;
static GLuint g_prog, g_vbo;
static GLint g_u_rect, g_u_color;
static int g_w, g_h; static GLuint g_blit_fbo; int g_android_rendering;
int g_android_surface_w = 1280, g_android_surface_h = 720;      /* read by GetClientRect/GetSystemMetrics */

static const char *VS = "#version 300 es\nuniform vec4 rect;layout(location=0) in vec2 p;\n"
    "void main(){vec2 q=rect.xy+p*rect.zw;gl_Position=vec4(q.x*2.0-1.0,1.0-q.y*2.0,0,1);}\n";
static const char *FS = "#version 300 es\nprecision mediump float;uniform vec4 color;out vec4 o;\n"
    "void main(){o=color;}\n";

static GLuint compile(GLenum t, const char *s)
{
    GLuint h = glCreateShader(t); glShaderSource(h, 1, &s, NULL); glCompileShader(h);
    GLint ok = 0; glGetShaderiv(h, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(h, sizeof(log), NULL, log); LOG("shader: %s", log); }
    return h;
}

int android_gl_ready(void) { return g_surf != EGL_NO_SURFACE; }

int android_gl_attach(ANativeWindow *w)
{
    if (!w) return 0;
    g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_dpy == EGL_NO_DISPLAY || !eglInitialize(g_dpy, NULL, NULL)) return 0;
    const EGLint cfg_attr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE };
    EGLConfig cfg; EGLint n = 0;
    if (!eglChooseConfig(g_dpy, cfg_attr, &cfg, 1, &n) || n < 1) return 0;
    EGLint fmt; eglGetConfigAttrib(g_dpy, cfg, EGL_NATIVE_VISUAL_ID, &fmt);
    ANativeWindow_setBuffersGeometry(w, 0, 0, fmt);
    g_surf = eglCreateWindowSurface(g_dpy, cfg, w, NULL);
    if (g_ctx == EGL_NO_CONTEXT) {
        const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    }
    if (g_surf == EGL_NO_SURFACE || g_ctx == EGL_NO_CONTEXT || !eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) return 0;
    eglQuerySurface(g_dpy, g_surf, EGL_WIDTH, &g_w); eglQuerySurface(g_dpy, g_surf, EGL_HEIGHT, &g_h);
    android_input_set_screen(g_w, g_h);
    g_android_surface_w = g_w; g_android_surface_h = g_h;
    eglSwapInterval(g_dpy, 1);

    if (!g_prog) {
        GLuint vs = compile(GL_VERTEX_SHADER, VS), fs = compile(GL_FRAGMENT_SHADER, FS);
        g_prog = glCreateProgram(); glAttachShader(g_prog, vs); glAttachShader(g_prog, fs); glLinkProgram(g_prog);
        g_u_rect = glGetUniformLocation(g_prog, "rect"); g_u_color = glGetUniformLocation(g_prog, "color");
        const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
        glGenBuffers(1, &g_vbo); glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    }
    LOG("EGL ready %dx%d, %s", g_w, g_h, (const char *)glGetString(GL_RENDERER));
    return 1;
}

void android_gl_detach(void)
{
    if (g_dpy == EGL_NO_DISPLAY) return;
    eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (g_surf != EGL_NO_SURFACE) eglDestroySurface(g_dpy, g_surf);
    g_surf = EGL_NO_SURFACE;
}

static void rect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glUniform4f(g_u_rect, x, y, w, h); glUniform4f(g_u_color, r, g, b, a);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void draw_overlay(void)
{
    glUseProgram(g_prog); glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const float ar = (float)g_w / (float)g_h;
    for (int i = 0; i < ANDROID_TOUCH_BUTTONS; ++i) {
        const android_touch_btn_t *b = &k_buttons[i];
        float a = android_input_overlay_pressed(b->id) ? 0.55f : 0.22f;
        rect(b->x - b->r / ar, b->y - b->r, 2 * b->r / ar, 2 * b->r, 1, 1, 1, a);
    }
    rect(k_stick_cx - k_stick_r / ar, k_stick_cy - k_stick_r, 2 * k_stick_r / ar, 2 * k_stick_r, 1, 1, 1, 0.12f);
}

/* Status screen shown until the game starts rendering. */
void android_gl_present(const char *status, float progress)
{
    (void)status;
    if (g_surf == EGL_NO_SURFACE || g_android_rendering) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, g_w, g_h); glDisable(GL_SCISSOR_TEST); glColorMask(1, 1, 1, 1);
    glClearColor(0.04f, 0.05f, 0.07f, 1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_prog); glBindBuffer(GL_ARRAY_BUFFER, g_vbo); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    rect(0.25f, 0.48f, 0.5f, 0.03f, 0.15f, 0.17f, 0.2f, 1);
    rect(0.25f, 0.48f, 0.5f * (progress < 0 ? 0 : progress > 1 ? 1 : progress), 0.03f, 0.3f, 0.7f, 0.4f, 1);
    draw_overlay(); eglSwapBuffers(g_dpy, g_surf);
}

/* D3d11GlesHost.present: the layer's back buffer is stored top row first, so the blit
 * flips it, and it is letterboxed to keep the game's aspect ratio. */
void android_gl_present_texture(void *user, uint32_t tex, int w, int h, int sync_interval)
{
    (void)user; if (g_surf == EGL_NO_SURFACE) return;
    g_android_rendering = 1;
    if (!g_blit_fbo) glGenFramebuffers(1, &g_blit_fbo);
    glDisable(GL_SCISSOR_TEST); glColorMask(1, 1, 1, 1);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_blit_fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0); glReadBuffer(GL_COLOR_ATTACHMENT0);
    float sa = (float)w / (float)h, da = (float)g_w / (float)g_h; int dw = g_w, dh = g_h;
    if (sa > da) dh = (int)((float)g_w / sa); else dw = (int)((float)g_h * sa);
    int x0 = (g_w - dw) / 2, y0 = (g_h - dh) / 2;
    glBlitFramebuffer(0, 0, w, h, x0, y0 + dh, x0 + dw, y0, GL_COLOR_BUFFER_BIT, GL_LINEAR);   /* dst y inverted = vertical flip */
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, g_w, g_h);
    draw_overlay(); eglSwapInterval(g_dpy, sync_interval > 0 ? 1 : 0); eglSwapBuffers(g_dpy, g_surf);
}
