/* Run the real GLES state application with stubbed GL entry points. */
#include <assert.h>
#include <stdio.h>
#include "../app/src/main/cpp/d3d11_gles/d3d11_gles.c"
static int calls;
#define V1(n,t) void n(t a) { (void)a; ++calls; }
#define V2(n,t,u) void n(t a,u b) { (void)a; (void)b; ++calls; }
#define V4(n,t,u,v,w) void n(t a,u b,v c,w d) { (void)a; (void)b; (void)c; (void)d; ++calls; }
V1(glEnable,GLenum) V1(glDisable,GLenum) V1(glDepthFunc,GLenum)
V1(glDepthMask,GLboolean) V1(glCullFace,GLenum) V1(glFrontFace,GLenum)
V2(glDepthRangef,GLfloat,GLfloat) V2(glPolygonOffset,GLfloat,GLfloat)
V2(glStencilMaskSeparate,GLenum,GLuint) V2(glBlendEquationSeparate,GLenum,GLenum)
V4(glViewport,GLint,GLint,GLsizei,GLsizei) V4(glScissor,GLint,GLint,GLsizei,GLsizei)
V4(glColorMask,GLboolean,GLboolean,GLboolean,GLboolean)
V4(glStencilFuncSeparate,GLenum,GLenum,GLint,GLuint)
V4(glStencilOpSeparate,GLenum,GLenum,GLenum,GLenum)
V4(glBlendFuncSeparate,GLenum,GLenum,GLenum,GLenum)
V4(glBlendColor,GLfloat,GLfloat,GLfloat,GLfloat)
int main(void)
{
    Ctx c; StateObj raster, depth, blend;
    memset(&c, 0, sizeof(c)); memset(&raster, 0, sizeof(raster));
    memset(&depth, 0, sizeof(depth)); memset(&blend, 0, sizeof(blend));
    apply_state(&c, 640, 480); assert(calls > 0); int first = calls;
    apply_state(&c, 640, 480); assert(calls == first);
    apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.nvp = 1; c.vp[0] = (D3D11_VIEWPORT){0,0,320,240,0,1};
    apply_state(&c, 320, 240); assert(calls == first); /* equivalent viewport */
    c.vp[0].Width = 300; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.rs = &raster; raster.u.rs.ScissorEnable = TRUE; c.nsc = 1;
    c.sc[0] = (D3D11_RECT){0,0,10,10}; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.sc[0].right = 9; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.dss = &depth; depth.u.ds.DepthEnable = TRUE; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.stencil_ref = 1; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.blend = &blend; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.blend_factor[0] = 0.5f; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    invalidate_att(); apply_state(&c, 320, 240); assert(calls > first);
    puts("draw state: reuse, changes and external-state invalidation OK"); return 0;
}
