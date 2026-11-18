/* Exercise compressed staging subregions, including nonzero block rows. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../app/src/main/cpp/d3d11_gles/d3d11_gles.c"
static unsigned char upload[64];
static int large_calls, large_rows;
void glActiveTexture(GLenum x) { (void)x; }
void glBindTexture(GLenum x, GLuint y) { (void)x;(void)y; }
void glPixelStorei(GLenum x, GLint y) { (void)x;(void)y; }
void glTexSubImage2D(GLenum t, GLint m, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void *p)
{ (void)t;(void)m;(void)x;(void)f;(void)ty;
  if (w==4 && h==4) { memcpy(upload,p,64); return; }
  assert(w==1024 && y==large_rows && h>0 && h<=256);
  const unsigned char *pixels=(const unsigned char *)p;
  for (int r=0;r<h;r++) {
      unsigned char expected = ((y+r)/4)%2 ? 255 : 0;
      assert(pixels[(size_t)r*w*4] == expected);
  }
  large_rows += h; ++large_calls;
}
GLenum glGetError(void) { return GL_NO_ERROR; }
void glBindFramebuffer(GLenum a, GLuint b) { (void)a;(void)b; }
void glFramebufferTexture2D(GLenum a, GLenum b, GLenum c, GLuint d, GLint e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
void glReadBuffer(GLenum a) { (void)a; }
void glReadPixels(GLint a, GLint b, GLsizei c, GLsizei d, GLenum e, GLenum f, void *g) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; }
void glBindBuffer(GLenum a, GLuint b) { (void)a;(void)b; }
void glCopyBufferSubData(GLenum a, GLenum b, GLintptr c, GLintptr d, GLsizeiptr e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
void glDisable(GLenum a) { (void)a; }
void glColorMask(GLboolean a, GLboolean b, GLboolean c, GLboolean d) { (void)a;(void)b;(void)c;(void)d; }
void glDepthMask(GLboolean a) { (void)a; }
void glStencilMaskSeparate(GLenum a, GLuint b) { (void)a;(void)b; }
void glDrawBuffers(GLsizei a, const GLenum *b) { (void)a;(void)b; }
void glBlitFramebuffer(GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield i, GLenum j) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j; }
int main(void)
{
    Tex src={0}, dst={0}, gpu={0}; unsigned char s[32]={0}, d[32]={0};
    unsigned char *sp[]={s}, *dp[]={d};
    src.o.kind=dst.o.kind=gpu.o.kind=K_TEX;
    src.staging=dst.staging=1; src.cpu=sp; dst.cpu=dp;
    src.fmt=dst.fmt=gpu.fmt=find_fmt_raw(DXGI_FORMAT_BC1_UNORM);
    src.mips=dst.mips=gpu.mips=1; src.nsub=dst.nsub=gpu.nsub=1;
    src.d.Width=dst.d.Width=gpu.d.Width=8; src.d.Height=dst.d.Height=gpu.d.Height=8;
    gpu.tex=3; gpu.target=GL_TEXTURE_2D;
    /* Block at x=4,y=4: opaque red in RGB565, other blocks stay black. */
    s[24]=0x00; s[25]=0xf8; s[26]=0x00; s[27]=0xf8;
    D3D11_BOX box={4,4,0,8,8,1};
    d3dgl_Ctx_CopySubresourceRegion(NULL,(ID3D11Resource *)&dst,0,0,4,0,(ID3D11Resource *)&src,0,&box);
    assert(!memcmp(d+16,s+24,8));
    for (int i=0;i<16;i++) assert(d[i]==0);
    d3dgl_Ctx_CopySubresourceRegion(NULL,(ID3D11Resource *)&gpu,0,0,0,0,(ID3D11Resource *)&src,0,&box);
    for (int i=0;i<16;i++) assert(upload[i*4]==255 && upload[i*4+1]==0 && upload[i*4+2]==0 && upload[i*4+3]==255);
    /* Alternating compressed block rows verify chunk source offsets and a
     * partial final chunk, not merely that multiple uploads were issued. */
    const unsigned w=1024, h=516, pitch=(w/4)*8;
    unsigned char *bc=calloc((h+3)/4, pitch); assert(bc);
    for(unsigned row=0;row<(h+3)/4;row++) if(row&1)
        for(unsigned col=0;col<w/4;col++) {
            unsigned char *block=bc+(size_t)row*pitch+col*8;
            block[0]=block[2]=0; block[1]=block[3]=0xf8;
        }
    gpu.d.Width=w; gpu.d.Height=h;
    tex_upload(&gpu,0,0,0,w,h,bc,pitch);
    assert(large_calls==3 && large_rows==(int)h);
    free(bc);
    puts("texture chunks: compressed staging regions and bounded decode OK");
}
