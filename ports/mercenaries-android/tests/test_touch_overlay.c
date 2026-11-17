/* Test the real overlay draw code with a hostile set of guest GL states. */
#include <assert.h>
#include <stdio.h>
#include "../app/src/main/cpp/android/android_gl.c"

const android_touch_btn_t k_buttons[ANDROID_TOUCH_BUTTONS] = {
    {"A",.90f,.78f,.065f,0}, {"B",.96f,.64f,.065f,1},
    {"X",.84f,.62f,.065f,2}, {"Y",.90f,.46f,.065f,3},
    {"L",.08f,.12f,.07f,4}, {"R",.92f,.12f,.07f,5},
    {"BLK",.72f,.88f,.045f,6}, {"WHT",.64f,.88f,.045f,7},
    {"STA",.54f,.90f,.04f,100}, {"BCK",.46f,.90f,.04f,101}
};
const float k_stick_cx=.16f, k_stick_cy=.70f, k_stick_r=.13f;
static int pressed;
int android_input_overlay_pressed(int id) { return pressed && id == 0; }

static int depth=1, stencil=1, cull=1, scissor=1, discard=1, coverage=1, offset=1, blend;
static int color_ok, equation_ok, viewport_ok, draws, uploads;
static GLuint bound_vao, used_program;
void glDisable(GLenum e) {
    switch(e) {
    case GL_DEPTH_TEST: depth=0; break; case GL_STENCIL_TEST: stencil=0; break;
    case GL_CULL_FACE: cull=0; break; case GL_SCISSOR_TEST: scissor=0; break;
    case GL_RASTERIZER_DISCARD: discard=0; break; case GL_SAMPLE_ALPHA_TO_COVERAGE: coverage=0; break;
    case GL_POLYGON_OFFSET_FILL: offset=0; break;
    }
}
void glEnable(GLenum e) { if (e == GL_BLEND) blend=1; }
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { viewport_ok = x==0 && y==0 && w==1920 && h==1080; }
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { color_ok = r && g && b && a; }
void glUseProgram(GLuint p) { used_program=p; }
void glBindVertexArray(GLuint a) { bound_vao=a; }
void glBlendEquationSeparate(GLenum a, GLenum b) { equation_ok = a==GL_FUNC_ADD && b==GL_FUNC_ADD; }
void glBlendFunc(GLenum a, GLenum b) { assert(a==GL_SRC_ALPHA && b==GL_ONE_MINUS_SRC_ALPHA); }
void glBindBuffer(GLenum target, GLuint buffer) { assert(target == GL_ARRAY_BUFFER && buffer == 43); }
void glBufferData(GLenum target, GLsizeiptr bytes, const void *data, GLenum usage) {
    assert(target == GL_ARRAY_BUFFER && data && usage == GL_DYNAMIC_DRAW);
    assert(bytes > (GLsizeiptr)((ANDROID_TOUCH_BUTTONS + 1) * 6 * sizeof(OverlayVertex)));
    ++uploads;
}
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    assert(mode==GL_TRIANGLES && first==0 && count>(ANDROID_TOUCH_BUTTONS + 1)*6 && count%6==0);
    assert(!depth && !stencil && !cull && !scissor && !discard && !coverage && !offset);
    assert(blend && color_ok && equation_ok && viewport_ok && bound_vao==41 && used_program==42);
    ++draws;
}
int main(void) {
    g_w=1920; g_h=1080; g_overlay_vao=41; g_prog=42; g_vbo=43;
    draw_overlay();
    assert(draws == 1 && uploads == 1);
    assert(g_overlay_vertices[0].a == 0.32f);
    draw_overlay();
    assert(draws == 2 && uploads == 1); /* no upload when state is unchanged */
    pressed = 1;
    draw_overlay();
    assert(draws == 3 && uploads == 2); /* pressed color updates */
    assert(g_overlay_vertices[0].a == 0.65f);
    puts("touch overlay: one draw, cached geometry and isolated GL state");
    return 0;
}
