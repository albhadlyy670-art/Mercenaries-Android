/* Run the real GLES state application with stubbed GL entry points. */
#include <assert.h>
#include <stdio.h>
#include "../app/src/main/cpp/d3d11_gles/d3d11_gles.c"
static int calls, destroyed, draw_buffers_calls;
static void destroy_mock(Obj *o) { (void)o; ++destroyed; }
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
void glBindFramebuffer(GLenum target, GLuint fbo) { (void)target; (void)fbo; }
void glFramebufferTexture2D(GLenum target, GLenum point, GLenum tex_target, GLuint tex, GLint level)
{ (void)target; (void)point; (void)tex_target; (void)tex; (void)level; }
void glDrawBuffers(GLsizei n, const GLenum *values) { assert(n > 0 && values); ++draw_buffers_calls; }
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
    /* Draw-buffer routing is FBO state: unchanged targets reuse it; clear,
     * presentation and copy invalidate it before the next draw. */
    Tex tex; View view; int width, height;
    memset(&tex, 0, sizeof(tex)); memset(&view, 0, sizeof(view));
    tex.d.Width = 640; tex.d.Height = 480; tex.tex = 1; view.tex = &tex;
    c.rtv[0] = &view; invalidate_att();
    bind_ctx_fbo(&c, &width, &height); assert(width == 640 && height == 480 && draw_buffers_calls == 1);
    bind_ctx_fbo(&c, &width, &height); assert(draw_buffers_calls == 1);
    c.rtv[0] = NULL; bind_ctx_fbo(&c, &width, &height); assert(draw_buffers_calls == 2);
    invalidate_att(); bind_ctx_fbo(&c, &width, &height); assert(draw_buffers_calls == 3);
    /* Repeated binding of the same object must retain it exactly once while
     * still updating independent vertex-buffer offsets and strides. */
    Ctx bindings; memset(&bindings, 0, sizeof(bindings));
    Buf *buf = new_obj(sizeof(Buf), K_BUFFER, destroy_mock);
    ID3D11Buffer *handle = (ID3D11Buffer *)buf;
    UINT stride = 8, offset = 4;
    d3dgl_Ctx_IASetVertexBuffers((ID3D11DeviceContext *)&bindings, 0, 1, &handle, &stride, &offset);
    assert(buf->o.ref == 2); d3dgl_Release(buf); assert(buf->o.ref == 1);
    stride = 16; offset = 12;
    d3dgl_Ctx_IASetVertexBuffers((ID3D11DeviceContext *)&bindings, 0, 1, &handle, &stride, &offset);
    assert(buf->o.ref == 1 && bindings.vb[0].stride == 16 && bindings.vb[0].offset == 12);
    handle = NULL;
    d3dgl_Ctx_IASetVertexBuffers((ID3D11DeviceContext *)&bindings, 0, 1, &handle, &stride, &offset);
    assert(destroyed == 1 && bindings.vb[0].b == NULL);
    puts("draw state: reuse, changes, invalidation and binding references OK"); return 0;
}
