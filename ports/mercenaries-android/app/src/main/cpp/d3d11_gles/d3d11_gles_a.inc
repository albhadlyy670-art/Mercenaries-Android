/* ===== part A: objects, formats, conversions, textures, buffers =========== */
#include "d3d11_gles_types.h"
#include "gles_gl.h"
#include "vertex_attrib_state.h"
#ifdef __ANDROID__
#include "nv2a/nv2a_mmio_perf.h"
#endif
#include "hlsl2glsl.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#if defined(__ANDROID__)
#include <android/log.h>
#include <EGL/egl.h>
#define D3DGL_LOG(...) __android_log_print(ANDROID_LOG_WARN, "d3d11gles", __VA_ARGS__)
#else
#define D3DGL_LOG(...) do { fprintf(stderr, "[d3d11gles] "); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)
#endif

enum { K_DEVICE = 1, K_CTX, K_BUFFER, K_TEX, K_SRV, K_RTV, K_DSV, K_VS, K_PS, K_GS, K_LAYOUT, K_BLEND, K_DS, K_RS, K_SAMP, K_BLOB, K_SC, K_FACTORY, K_ADAPTER, K_DXGIDEV };
typedef struct Obj { uint32_t magic; int kind; int ref; void (*destroy)(struct Obj *); } Obj;
#define OBJ_MAGIC 0xD3D611E5u

static void *new_obj(size_t size, int kind, void (*destroy)(Obj *))
{
    Obj *o = (Obj *)calloc(1, size);
    if (!o) return NULL;
    o->magic = OBJ_MAGIC; o->kind = kind; o->ref = 1; o->destroy = destroy; return o;
}
ULONG d3dgl_AddRef(void *p)
{
    Obj *o = (Obj *)p; if (!o) return 0;
    return (ULONG)__atomic_add_fetch(&o->ref, 1, __ATOMIC_SEQ_CST);
}
ULONG d3dgl_Release(void *p)
{
    Obj *o = (Obj *)p; if (!o) return 0;
    int r = __atomic_sub_fetch(&o->ref, 1, __ATOMIC_SEQ_CST);
    if (r == 0) { if (o->destroy) o->destroy(o); o->magic = 0; free(o); }
    return (ULONG)(r < 0 ? 0 : r);
}
#define REF(p) ((void)d3dgl_AddRef(p))
/* replace a held reference */
#define SETREF(dst, src) do { void *_n = (src); void *_o = (dst); if (_n != _o) { if (_n) d3dgl_AddRef(_n); (dst) = _n; if (_o) d3dgl_Release(_o); } } while (0)

/* ---- half float ----------------------------------------------------------------- */
static uint16_t f2h(float f)
{
    union { float f; uint32_t u; } v = { f }; uint32_t x = v.u;
    uint32_t sign = (x >> 16) & 0x8000u; int32_t e = (int32_t)((x >> 23) & 0xFF) - 127 + 15; uint32_t m = x & 0x7FFFFFu;
    if (e <= 0) { if (e < -10) return (uint16_t)sign; m |= 0x800000u; uint32_t t = m >> (1 - e + 13); if ((m >> (12 - e)) & 1u) t++; return (uint16_t)(sign | t); }
    if (e >= 31) return (uint16_t)(sign | 0x7C00u | (m ? 0x200u : 0));
    uint32_t h = sign | ((uint32_t)e << 10) | (m >> 13); if (m & 0x1000u) h++; return (uint16_t)h;
}
static float h2f(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000u) << 16, e = (h >> 10) & 0x1Fu, m = h & 0x3FFu; union { uint32_t u; float f; } v;
    if (e == 0) { if (!m) { v.u = sign; return v.f; } while (!(m & 0x400u)) { m <<= 1; e--; } e++; m &= 0x3FFu; }
    else if (e == 31) { v.u = sign | 0x7F800000u | (m << 13); return v.f; }
    v.u = sign | ((e + 112) << 23) | (m << 13); return v.f;
}

/* ---- format table ------------------------------------------------------------------ */
enum { CV_NONE, CV_SWAP_RB, CV_SWAP_RB_X, CV_B5G5R5A1, CV_B4G4R4A4, CV_BC1, CV_BC2, CV_BC3, CV_SNORM16, CV_D24S8 };
typedef struct {
    DXGI_FORMAT dxgi; GLenum internal, format, type;
    uint8_t d3d_bpp, gl_bpp, conv, comps, depth, stencil, integer, a8, rt_ok;
} Fmt;
#define F(d, i, f, t, db, gb, cv, c, dep, st, in, a8, rt) { d, i, f, t, db, gb, cv, c, dep, st, in, a8, rt }
static const Fmt k_fmts[] = {
    F(DXGI_FORMAT_R8G8B8A8_UNORM,      GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 4, CV_NONE, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_R8G8B8A8_TYPELESS,   GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 4, CV_NONE, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 4, CV_NONE, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_B8G8R8A8_UNORM,      GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 4, CV_SWAP_RB, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_B8G8R8X8_UNORM,      GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 4, CV_SWAP_RB_X, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_B5G6R5_UNORM,        GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2, 2, CV_NONE, 3, 0,0,0,0,1),
    F(DXGI_FORMAT_B5G5R5A1_UNORM,      GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 2, 4, CV_B5G5R5A1, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_B4G4R4A4_UNORM,      GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 2, 4, CV_B4G4R4A4, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_R10G10B10A2_UNORM,   GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4, 4, CV_NONE, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_R8_UNORM,            GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1, 1, CV_NONE, 1, 0,0,0,0,1),
    F(DXGI_FORMAT_A8_UNORM,            GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1, 1, CV_NONE, 1, 0,0,0,1,0),
    F(DXGI_FORMAT_R8G8_UNORM,          GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2, 2, CV_NONE, 2, 0,0,0,0,1),
    F(DXGI_FORMAT_R16_SNORM,           GL_R16F, GL_RED, GL_HALF_FLOAT, 2, 2, CV_SNORM16, 1, 0,0,0,0,1),
    F(DXGI_FORMAT_R16G16_SNORM,        GL_RG16F, GL_RG, GL_HALF_FLOAT, 4, 4, CV_SNORM16, 2, 0,0,0,0,1),
    F(DXGI_FORMAT_R16G16B16A16_SNORM,  GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8, 8, CV_SNORM16, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_R16_FLOAT,           GL_R16F, GL_RED, GL_HALF_FLOAT, 2, 2, CV_NONE, 1, 0,0,0,0,1),
    F(DXGI_FORMAT_R16G16B16A16_FLOAT,  GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8, 8, CV_NONE, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_R16_UINT,            GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, 2, 2, CV_NONE, 1, 0,0,1,0,1),
    F(DXGI_FORMAT_R16_SINT,            GL_R16I, GL_RED_INTEGER, GL_SHORT, 2, 2, CV_NONE, 1, 0,0,1,0,1),
    F(DXGI_FORMAT_R16G16_SINT,         GL_RG16I, GL_RG_INTEGER, GL_SHORT, 4, 4, CV_NONE, 2, 0,0,1,0,1),
    F(DXGI_FORMAT_R16G16B16A16_SINT,   GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, 8, 8, CV_NONE, 4, 0,0,1,0,1),
    F(DXGI_FORMAT_R32_UINT,            GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4, 4, CV_NONE, 1, 0,0,1,0,1),
    F(DXGI_FORMAT_R32_SINT,            GL_R32I, GL_RED_INTEGER, GL_INT, 4, 4, CV_NONE, 1, 0,0,1,0,1),
    F(DXGI_FORMAT_R32_FLOAT,           GL_R32F, GL_RED, GL_FLOAT, 4, 4, CV_NONE, 1, 0,0,0,0,1),
    F(DXGI_FORMAT_R32G32_FLOAT,        GL_RG32F, GL_RG, GL_FLOAT, 8, 8, CV_NONE, 2, 0,0,0,0,1),
    F(DXGI_FORMAT_R32G32B32A32_FLOAT,  GL_RGBA32F, GL_RGBA, GL_FLOAT, 16, 16, CV_NONE, 4, 0,0,0,0,1),
    F(DXGI_FORMAT_BC1_UNORM,           GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 0, 4, CV_BC1, 4, 0,0,0,0,0),
    F(DXGI_FORMAT_BC2_UNORM,           GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 0, 4, CV_BC2, 4, 0,0,0,0,0),
    F(DXGI_FORMAT_BC3_UNORM,           GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 0, 4, CV_BC3, 4, 0,0,0,0,0),
    F(DXGI_FORMAT_D24_UNORM_S8_UINT,   GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4, 4, CV_D24S8, 1, 1,1,0,0,0),
    F(DXGI_FORMAT_D16_UNORM,           GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 2, 2, CV_NONE, 1, 1,0,0,0,0),
    F(DXGI_FORMAT_D32_FLOAT,           GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4, 4, CV_NONE, 1, 1,0,0,0,0),
};
#undef F
static const Fmt *find_fmt_raw(DXGI_FORMAT f) { for (size_t i = 0; i < sizeof(k_fmts) / sizeof(k_fmts[0]); ++i) if (k_fmts[i].dxgi == f) return &k_fmts[i]; return NULL; }
/* Resolve typeless/view formats of depth resources. */
static const Fmt *resource_fmt(DXGI_FORMAT f, UINT bind)
{
    if (f == DXGI_FORMAT_R24G8_TYPELESS) return find_fmt_raw(DXGI_FORMAT_D24_UNORM_S8_UINT);
    if (f == DXGI_FORMAT_R32_TYPELESS) return find_fmt_raw((bind & D3D11_BIND_DEPTH_STENCIL) ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_R32_FLOAT);
    return find_fmt_raw(f);
}
static int is_bc(const Fmt *f) { return f->conv == CV_BC1 || f->conv == CV_BC2 || f->conv == CV_BC3; }
static UINT d3d_row_bytes(const Fmt *f, UINT w) { return is_bc(f) ? ((w + 3) / 4) * (f->conv == CV_BC1 ? 8u : 16u) : w * f->d3d_bpp; }
static UINT d3d_rows(const Fmt *f, UINT h) { return is_bc(f) ? (h + 3) / 4 : h; }

/* ---- BC decode --------------------------------------------------------------------------- */
static void bc_color_block(const uint8_t *b, uint8_t out[16][4], int force4)
{
    uint16_t c0 = (uint16_t)(b[0] | (b[1] << 8)), c1 = (uint16_t)(b[2] | (b[3] << 8)); uint8_t col[4][4];
    uint16_t c[2] = { c0, c1 };
    for (int i = 0; i < 2; ++i) { col[i][0] = (uint8_t)(((c[i] >> 11) & 31) * 255 / 31); col[i][1] = (uint8_t)(((c[i] >> 5) & 63) * 255 / 63); col[i][2] = (uint8_t)((c[i] & 31) * 255 / 31); col[i][3] = 255; }
    if (c0 > c1 || force4) {
        for (int k = 0; k < 3; ++k) { col[2][k] = (uint8_t)((2 * col[0][k] + col[1][k]) / 3); col[3][k] = (uint8_t)((col[0][k] + 2 * col[1][k]) / 3); }
        col[2][3] = col[3][3] = 255;
    } else {
        for (int k = 0; k < 3; ++k) col[2][k] = (uint8_t)((col[0][k] + col[1][k]) / 2);
        col[2][3] = 255; col[3][0] = col[3][1] = col[3][2] = col[3][3] = 0;
    }
    uint32_t idx = (uint32_t)b[4] | ((uint32_t)b[5] << 8) | ((uint32_t)b[6] << 16) | ((uint32_t)b[7] << 24);
    for (int i = 0; i < 16; ++i) memcpy(out[i], col[(idx >> (2 * i)) & 3], 4);
}
static void bc_decode_block(int conv, const uint8_t *b, uint8_t out[16][4])
{
    if (conv == CV_BC1) { bc_color_block(b, out, 0); return; }
    bc_color_block(b + 8, out, 1);
    if (conv == CV_BC2) {
        for (int i = 0; i < 16; ++i) { int v = (b[i / 2] >> ((i & 1) * 4)) & 15; out[i][3] = (uint8_t)(v * 17); }
    } else {
        uint8_t a[8]; a[0] = b[0]; a[1] = b[1];
        if (a[0] > a[1]) for (int k = 1; k < 7; ++k) a[k + 1] = (uint8_t)(((7 - k) * a[0] + k * a[1]) / 7);
        else { for (int k = 1; k < 5; ++k) a[k + 1] = (uint8_t)(((5 - k) * a[0] + k * a[1]) / 5); a[6] = 0; a[7] = 255; }
        uint64_t bits = 0; for (int i = 0; i < 6; ++i) bits |= (uint64_t)b[2 + i] << (8 * i);
        for (int i = 0; i < 16; ++i) out[i][3] = a[(bits >> (3 * i)) & 7];
    }
}

/* ---- pixel conversion D3D bytes <-> GL bytes ------------------------------------------------- */
static void convert_to_gl(const Fmt *f, const uint8_t *src, UINT src_pitch, UINT w, UINT h, uint8_t *dst)
{
    UINT dbpp = f->gl_bpp;
    if (is_bc(f)) {
        for (UINT by = 0; by < (h + 3) / 4; ++by) for (UINT bx = 0; bx < (w + 3) / 4; ++bx) {
            uint8_t px[16][4]; bc_decode_block(f->conv, src + by * src_pitch + bx * (f->conv == CV_BC1 ? 8 : 16), px);
            for (int j = 0; j < 4; ++j) for (int i = 0; i < 4; ++i) {
                UINT x = bx * 4 + (UINT)i, y = by * 4 + (UINT)j; if (x < w && y < h) memcpy(dst + ((size_t)y * w + x) * 4, px[j * 4 + i], 4);
            }
        }
        return;
    }
    for (UINT y = 0; y < h; ++y) {
        const uint8_t *s = src + (size_t)y * src_pitch; uint8_t *d = dst + (size_t)y * w * dbpp;
        switch (f->conv) {
        case CV_NONE: memcpy(d, s, (size_t)w * f->d3d_bpp); break;
        case CV_SWAP_RB: case CV_SWAP_RB_X:
            for (UINT x = 0; x < w; ++x) { d[4*x] = s[4*x+2]; d[4*x+1] = s[4*x+1]; d[4*x+2] = s[4*x]; d[4*x+3] = f->conv == CV_SWAP_RB_X ? 255 : s[4*x+3]; } break;
        case CV_B5G5R5A1:
            for (UINT x = 0; x < w; ++x) { uint16_t p = (uint16_t)(s[2*x] | (s[2*x+1] << 8)); d[4*x] = (uint8_t)(((p >> 10) & 31) * 255 / 31); d[4*x+1] = (uint8_t)(((p >> 5) & 31) * 255 / 31); d[4*x+2] = (uint8_t)((p & 31) * 255 / 31); d[4*x+3] = (p & 0x8000u) ? 255 : 0; } break;
        case CV_B4G4R4A4:
            for (UINT x = 0; x < w; ++x) { uint16_t p = (uint16_t)(s[2*x] | (s[2*x+1] << 8)); d[4*x] = (uint8_t)(((p >> 8) & 15) * 17); d[4*x+1] = (uint8_t)(((p >> 4) & 15) * 17); d[4*x+2] = (uint8_t)((p & 15) * 17); d[4*x+3] = (uint8_t)(((p >> 12) & 15) * 17); } break;
        case CV_SNORM16:
            for (UINT x = 0; x < w * f->comps; ++x) { int16_t v = (int16_t)(s[2*x] | (s[2*x+1] << 8)); float fl = v / 32767.0f; if (fl < -1.f) fl = -1.f; uint16_t hv = f2h(fl); d[2*x] = (uint8_t)hv; d[2*x+1] = (uint8_t)(hv >> 8); } break;
        case CV_D24S8:
            for (UINT x = 0; x < w; ++x) { uint32_t p; memcpy(&p, s + 4*x, 4); p = (p << 8) | (p >> 24); memcpy(d + 4*x, &p, 4); } break;
        default: memcpy(d, s, (size_t)w * dbpp); break;
        }
    }
}
static void convert_from_gl(const Fmt *f, const uint8_t *src, UINT w, UINT h, uint8_t *dst, UINT dst_pitch)
{
    if (is_bc(f)) return;                                                         /* read-back of compressed data is not supported */
    for (UINT y = 0; y < h; ++y) {
        const uint8_t *s = src + (size_t)y * w * f->gl_bpp; uint8_t *d = dst + (size_t)y * dst_pitch;
        switch (f->conv) {
        case CV_NONE: memcpy(d, s, (size_t)w * f->d3d_bpp); break;
        case CV_SWAP_RB: case CV_SWAP_RB_X:
            for (UINT x = 0; x < w; ++x) { d[4*x] = s[4*x+2]; d[4*x+1] = s[4*x+1]; d[4*x+2] = s[4*x]; d[4*x+3] = s[4*x+3]; } break;
        case CV_B5G5R5A1:
            for (UINT x = 0; x < w; ++x) { uint16_t p = (uint16_t)(((s[4*x] >> 3) << 10) | ((s[4*x+1] >> 3) << 5) | (s[4*x+2] >> 3) | (s[4*x+3] >= 128 ? 0x8000 : 0)); d[2*x] = (uint8_t)p; d[2*x+1] = (uint8_t)(p >> 8); } break;
        case CV_B4G4R4A4:
            for (UINT x = 0; x < w; ++x) { uint16_t p = (uint16_t)(((s[4*x] >> 4) << 8) | ((s[4*x+1] >> 4) << 4) | (s[4*x+2] >> 4) | ((s[4*x+3] >> 4) << 12)); d[2*x] = (uint8_t)p; d[2*x+1] = (uint8_t)(p >> 8); } break;
        case CV_SNORM16:
            for (UINT x = 0; x < w * f->comps; ++x) { float fl = h2f((uint16_t)(s[2*x] | (s[2*x+1] << 8))); if (fl > 1.f) fl = 1.f; if (fl < -1.f) fl = -1.f; int16_t v = (int16_t)lrintf(fl * 32767.f); d[2*x] = (uint8_t)v; d[2*x+1] = (uint8_t)((uint16_t)v >> 8); } break;
        case CV_D24S8:
            for (UINT x = 0; x < w; ++x) { uint32_t p; memcpy(&p, s + 4*x, 4); p = (p >> 8) | (p << 24); memcpy(d + 4*x, &p, 4); } break;
        default: memcpy(d, s, (size_t)w * f->d3d_bpp); break;
        }
    }
}

/* ---- global device state -------------------------------------------------------------------- */
#define MAX_VB 16
#define MAX_CB 16
#define MAX_TEXSLOT 16
#define MAX_RT 8
#define MAX_VP 16
#define MAX_PROGRAMS 512

typedef struct Tex Tex; typedef struct Buf Buf; typedef struct Shader Shader; typedef struct Layout Layout; typedef struct Sampler Sampler;
typedef struct View View; typedef struct StateObj StateObj; typedef struct Program Program;

struct Tex {
    Obj o; D3D11_TEXTURE2D_DESC d; GLuint tex; GLenum target; const Fmt *fmt; int cube, mips, layers, staging, point_params;
    uint8_t **cpu; int nsub; int map_sub; uint8_t *map_buf; D3D11_MAP map_type;
};
#define D3DGL_RING_GENERATIONS 3
typedef struct { GLuint buf; uint8_t *ptr; void *fence; } D3dglRingGeneration;
struct Buf { Obj o; D3D11_BUFFER_DESC d; GLuint buf; uint8_t *tmp; D3D11_MAP map_type; int mapped_gl;
    uint8_t *persistent; int persistent_written, persistent_generation;
    D3dglRingGeneration generations[D3DGL_RING_GENERATIONS]; };
struct View { Obj o; Tex *tex; Buf *buf; D3D11_SHADER_RESOURCE_VIEW_DESC srv; int mip, stencil_view; DXGI_FORMAT view_fmt; };
struct Sampler { Obj o; D3D11_SAMPLER_DESC d; GLuint gl, gl_point; };
struct StateObj { Obj o; union { D3D11_BLEND_DESC blend; D3D11_DEPTH_STENCIL_DESC ds; D3D11_RASTERIZER_DESC rs; } u; };
struct Shader { Obj o; int stage, unsupported; HlslResult r; char *glsl; };
struct Layout { Obj o; D3D11_INPUT_ELEMENT_DESC el[32]; char names[32][32]; int n; int loc[32]; };

typedef struct { Buf *b; UINT stride, offset; } VBind;
typedef struct {
    Obj o;
    Shader *vs, *ps, *gs; Layout *layout; D3D11_PRIMITIVE_TOPOLOGY topo;
    VBind vb[MAX_VB]; Buf *ib; DXGI_FORMAT ib_fmt; UINT ib_off;
    Buf *cb_vs[MAX_CB], *cb_ps[MAX_CB], *cb_gs[MAX_CB];
    View *srv_ps[MAX_TEXSLOT], *srv_vs[MAX_TEXSLOT]; Sampler *smp_ps[MAX_TEXSLOT], *smp_vs[MAX_TEXSLOT];
    D3D11_VIEWPORT vp[MAX_VP]; UINT nvp; D3D11_RECT sc[MAX_VP]; UINT nsc;
    View *rtv[MAX_RT]; View *dsv; StateObj *blend, *dss, *rs; FLOAT blend_factor[4]; UINT sample_mask, stencil_ref;
} Ctx;

#define DRAW_FBO_CACHE_SIZE 64
/* GL names, mip levels and attachment points define immutable draw FBOs.
 * Texture destruction evicts references before a GL name can be reused. */
typedef struct { GLuint tex, target; GLint mip; } DrawFboAttachment;
typedef struct { DrawFboAttachment att[MAX_RT + 1]; GLenum depth_point; } DrawFboKey;
typedef struct { DrawFboKey key; GLuint fbo; } DrawFboEntry;
typedef struct { Shader *vs, *ps; Program *p; } ProgKey;
struct Program { GLuint prog; Shader *vs, *ps; int n_vs, n_ps; uint32_t bgra; int ps_unit0; };

typedef struct {
    int inited; Ctx *ctx; D3d11GlesHost host; int dbg;
    DrawFboEntry draw_fbos[DRAW_FBO_CACHE_SIZE]; unsigned draw_fbo_next;
    GLuint vao, fbo, read_fbo, draw_fbo; GLuint att_tex[MAX_RT + 1], att_target[MAX_RT + 1]; GLint att_level[MAX_RT + 1]; int fbo_dirty;
    ProgKey progs[MAX_PROGRAMS]; int nprogs; int last_program; int errors; int max_units;
    uint32_t enabled_attribs; int attribs_initialized;
    uint32_t divisor_valid; GLuint attrib_divisors[16];
    Ctx *fbo_ctx; int fbo_width, fbo_height;
    unsigned draw_buffer_mask; int draw_buffers_valid;
    GLuint current_program; int program_valid, vao_bound;
    GLuint copy_write_buffer; int copy_write_valid;
    GLuint uniform_buffers[32]; uint32_t uniform_valid;
    GLuint tex2d[32], texcube[32], samplers[32];
    uint32_t tex2d_valid, texcube_valid, sampler_valid;
    GLuint active_texture; int active_texture_valid;
} Global;
static Global G;

#ifdef __ANDROID__
/* Sample potentially blocking driver calls rather than timing every draw.
 * These are CPU-side durations, not GPU execution times. All rendering and
 * presentation occur on the context-owning game thread. */
#include <time.h>
void android_trace(const char *fmt, ...);
typedef struct { unsigned long long calls, samples; double seconds; } PerfOp;
static PerfOp perf_map, perf_unmap, perf_draw, perf_drawprep, perf_program, perf_fbstate, perf_fbo, perf_state, perf_resources, perf_vertex, perf_issue, perf_read, perf_ringmap, perf_ringunmap;
static double perf_start;
static unsigned long long perf_state_applies;
typedef struct {
    unsigned long long arrays, indexed, vertices, small, medium, large, huge;
    unsigned long long fbo_rebinds, attachments, ring_vertex_reuses, ring_index_reuses;
    unsigned long long persistent_uploads, persistent_wraps;
    double persistent_wait_seconds;
    unsigned long long small_vertexid, small_attributes, small_triangles, small_strips, small_other;
} PerfWorkload;
static PerfWorkload perf_workload;
static void perf_record_draw(UINT count, int indexed, int vertexid, D3D11_PRIMITIVE_TOPOLOGY topology)
{
    if (indexed) ++perf_workload.indexed; else ++perf_workload.arrays;
    perf_workload.vertices += count;
    if (count <= 16) {
        ++perf_workload.small;
        if (vertexid) ++perf_workload.small_vertexid; else ++perf_workload.small_attributes;
        if (topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) ++perf_workload.small_triangles;
        else if (topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP) ++perf_workload.small_strips;
        else ++perf_workload.small_other;
    }
    else if (count <= 64) ++perf_workload.medium;
    else if (count <= 256) ++perf_workload.large;
    else ++perf_workload.huge;
}
void d3d11gles_note_vertex_ring_reuse(void) { ++perf_workload.ring_vertex_reuses; }
void d3d11gles_note_index_ring_reuse(void) { ++perf_workload.ring_index_reuses; }
#define PERF_DRAW_WORK(count, indexed, vertexid, topology) perf_record_draw((count), (indexed), (vertexid), (topology))
#define PERF_FBO_REBIND() (++perf_workload.fbo_rebinds)
#define PERF_ATTACHMENT() (++perf_workload.attachments)
#define PERF_STATE_APPLY() (++perf_state_applies)
static double perf_now(void)
{
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC, &t) == 0 ? t.tv_sec + t.tv_nsec * 1e-9 : 0.0;
}
static double perf_begin(PerfOp *op)
{
    ++op->calls;
    return ((op->calls & 63u) == 1u) ? perf_now() : 0.0;
}
static void perf_end(PerfOp *op, double start)
{
    if (start > 0.0) { double end = perf_now(); if (end >= start) { op->seconds += end - start; ++op->samples; } }
}
#define PERF_BEGIN(name) double perf_stamp = perf_begin(&perf_##name)
#define PERF_END(name) perf_end(&perf_##name, perf_stamp)
static Nv2aMmioPerfCounts perf_mmio_last;
static void perf_report(void)
{
    double now = perf_now();
    if (now <= 0.0) return;
    if (perf_start == 0.0 || now < perf_start || now - perf_start > 30.0) { perf_mmio_last = nv2a_mmio_get_perf_counts(); perf_start = now; perf_state_applies = 0; memset(&perf_workload, 0, sizeof(perf_workload)); memset(&perf_map, 0, sizeof(perf_map)); memset(&perf_unmap, 0, sizeof(perf_unmap)); memset(&perf_draw, 0, sizeof(perf_draw)); memset(&perf_drawprep, 0, sizeof(perf_drawprep)); memset(&perf_program, 0, sizeof(perf_program)); memset(&perf_fbstate, 0, sizeof(perf_fbstate)); memset(&perf_fbo, 0, sizeof(perf_fbo)); memset(&perf_state, 0, sizeof(perf_state)); memset(&perf_resources, 0, sizeof(perf_resources)); memset(&perf_vertex, 0, sizeof(perf_vertex)); memset(&perf_issue, 0, sizeof(perf_issue)); memset(&perf_read, 0, sizeof(perf_read)); memset(&perf_ringmap, 0, sizeof(perf_ringmap)); memset(&perf_ringunmap, 0, sizeof(perf_ringunmap)); return; }
    if (now - perf_start < 10.0) return;
#define AVG(op) ((op).samples ? 1000.0 * (op).seconds / (op).samples : 0.0)
    android_trace("[perf-gles] interval_s=%.1f programs=%d state_applies=%llu draws=%llu draw_samples=%llu draw_avg_ms=%.3f maps=%llu map_samples=%llu map_avg_ms=%.3f unmaps=%llu unmap_samples=%llu unmap_avg_ms=%.3f reads=%llu read_samples=%llu read_avg_ms=%.3f ring_maps=%llu ring_map_samples=%llu ring_map_avg_ms=%.3f ring_unmaps=%llu ring_unmap_samples=%llu ring_unmap_avg_ms=%.3f",
        now - perf_start, G.nprogs, perf_state_applies, perf_draw.calls, perf_draw.samples, AVG(perf_draw), perf_map.calls, perf_map.samples, AVG(perf_map), perf_unmap.calls, perf_unmap.samples, AVG(perf_unmap), perf_read.calls, perf_read.samples, AVG(perf_read), perf_ringmap.calls, perf_ringmap.samples, AVG(perf_ringmap), perf_ringunmap.calls, perf_ringunmap.samples, AVG(perf_ringunmap));
    android_trace("[perf-gles-draw] prepare_samples=%llu prepare_avg_ms=%.3f issue_samples=%llu issue_avg_ms=%.3f",
        perf_drawprep.samples, AVG(perf_drawprep), perf_issue.samples, AVG(perf_issue));
    android_trace("[perf-gles-prepare] program_samples=%llu program_avg_ms=%.3f fbstate_samples=%llu fbstate_avg_ms=%.3f resources_samples=%llu resources_avg_ms=%.3f vertex_samples=%llu vertex_avg_ms=%.3f",
        perf_program.samples, AVG(perf_program), perf_fbstate.samples, AVG(perf_fbstate), perf_resources.samples, AVG(perf_resources), perf_vertex.samples, AVG(perf_vertex));
    android_trace("[perf-gles-fbstate] fbo_samples=%llu fbo_avg_ms=%.3f state_samples=%llu state_avg_ms=%.3f",
        perf_fbo.samples, AVG(perf_fbo), perf_state.samples, AVG(perf_state));
    android_trace("[perf-gles-workload] arrays=%llu indexed=%llu vertices=%llu draws_le16=%llu draws_le64=%llu draws_le256=%llu draws_gt256=%llu fbo_rebinds=%llu attachment_updates=%llu ring_vertex_reuses=%llu ring_index_reuses=%llu",
        perf_workload.arrays, perf_workload.indexed, perf_workload.vertices,
        perf_workload.small, perf_workload.medium, perf_workload.large, perf_workload.huge,
        perf_workload.fbo_rebinds, perf_workload.attachments, perf_workload.ring_vertex_reuses, perf_workload.ring_index_reuses);
    android_trace("[perf-gles-persistent] uploads=%llu wraps=%llu wait_total_ms=%.3f",
        perf_workload.persistent_uploads, perf_workload.persistent_wraps, perf_workload.persistent_wait_seconds * 1000.0);
    android_trace("[perf-gles-small] vertexid=%llu attributes=%llu triangle_list=%llu triangle_strip=%llu other=%llu",
        perf_workload.small_vertexid, perf_workload.small_attributes, perf_workload.small_triangles,
        perf_workload.small_strips, perf_workload.small_other);
    Nv2aMmioPerfCounts mmio_now = nv2a_mmio_get_perf_counts();
    Nv2aMmioPerfCounts mmio_interval = nv2a_mmio_perf_delta(mmio_now, perf_mmio_last);
    android_trace("[perf-mmio] interval_s=%.1f sigsegv_reads=%llu sigsegv_writes=%llu put_kicks=%llu",
        now - perf_start, (unsigned long long)mmio_interval.reads,
        (unsigned long long)mmio_interval.writes, (unsigned long long)mmio_interval.puts);
    perf_mmio_last = mmio_now;
#undef AVG
    perf_start = now; perf_state_applies = 0; memset(&perf_workload, 0, sizeof(perf_workload)); memset(&perf_map, 0, sizeof(perf_map)); memset(&perf_unmap, 0, sizeof(perf_unmap)); memset(&perf_draw, 0, sizeof(perf_draw)); memset(&perf_drawprep, 0, sizeof(perf_drawprep)); memset(&perf_program, 0, sizeof(perf_program)); memset(&perf_fbstate, 0, sizeof(perf_fbstate)); memset(&perf_fbo, 0, sizeof(perf_fbo)); memset(&perf_state, 0, sizeof(perf_state)); memset(&perf_resources, 0, sizeof(perf_resources)); memset(&perf_vertex, 0, sizeof(perf_vertex)); memset(&perf_issue, 0, sizeof(perf_issue)); memset(&perf_read, 0, sizeof(perf_read)); memset(&perf_ringmap, 0, sizeof(perf_ringmap)); memset(&perf_ringunmap, 0, sizeof(perf_ringunmap));
}
#else
void d3d11gles_note_vertex_ring_reuse(void) { }
void d3d11gles_note_index_ring_reuse(void) { }
#define PERF_STATE_APPLY() ((void)0)
#define PERF_DRAW_WORK(count, indexed, vertexid, topology) ((void)0)
#define PERF_FBO_REBIND() ((void)0)
#define PERF_ATTACHMENT() ((void)0)
#define PERF_BEGIN(name) ((void)0)
#define PERF_END(name) ((void)0)
#define perf_report() ((void)0)
#endif

void d3d11gles_set_host(const D3d11GlesHost *h) { if (h) G.host = *h; else memset(&G.host, 0, sizeof(G.host)); }
int d3d11gles_take_error_count(void) { int e = G.errors; G.errors = 0; return e; }
static void gl_check(const char *where)
{
    GLenum e; int n = 0;
    while ((e = glGetError()) != GL_NO_ERROR && n++ < 8) { G.errors++; if (G.dbg) D3DGL_LOG("GL error 0x%04X after %s", e, where); }
}

/* Per-operation checks in these hot paths cost multiple driver calls per draw.
 * On Android drain errors at Present instead; D3DGL_DEBUG=1 restores checks
 * after each operation when diagnosing graphics. */
static void gl_check_hot(const char *where)
{
#if defined(__ANDROID__) || defined(D3DGL_TEST_DEFER_HOT_ERRORS)
    if (G.dbg) gl_check(where);
#else
    gl_check(where);
#endif
}

static void invalidate_texture_bindings(void)
{
    G.tex2d_valid = G.texcube_valid = G.sampler_valid = 0;
    G.active_texture_valid = 0;
}
static void activate_texture_unit(GLuint unit)
{
    if (!G.active_texture_valid || G.active_texture != unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        G.active_texture = unit; G.active_texture_valid = 1;
    }
}
static void bind_texture_unit(GLuint unit, GLenum target, GLuint texture)
{
    if (unit >= 32u || (target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP)) {
        activate_texture_unit(unit); glBindTexture(target, texture); return;
    }
    uint32_t bit = 1u << unit;
    uint32_t *valid = target == GL_TEXTURE_2D ? &G.tex2d_valid : &G.texcube_valid;
    GLuint *entries = target == GL_TEXTURE_2D ? G.tex2d : G.texcube;
    if (!(*valid & bit) || entries[unit] != texture) {
        activate_texture_unit(unit); glBindTexture(target, texture);
        entries[unit] = texture; *valid |= bit;
    }
}
static void bind_sampler_unit(GLuint unit, GLuint sampler)
{
    if (unit >= 32u) { glBindSampler(unit, sampler); return; }
    uint32_t bit = 1u << unit;
    if (!(G.sampler_valid & bit) || G.samplers[unit] != sampler) {
        glBindSampler(unit, sampler); G.samplers[unit] = sampler; G.sampler_valid |= bit;
    }
}

/* ---- textures ---------------------------------------------------------------------------------- */
static UINT dimu(UINT v, UINT m) { v >>= m; return v ? v : 1; }
static void sub_info(const Tex *t, UINT sub, UINT *w, UINT *h, UINT *mip, UINT *layer)
{
    UINT m = sub % (UINT)t->mips, l = sub / (UINT)t->mips;
    if (w) *w = dimu(t->d.Width, m); if (h) *h = dimu(t->d.Height, m); if (mip) *mip = m; if (layer) *layer = l;
}
static GLenum face_target(const Tex *t, UINT layer) { return t->cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer : GL_TEXTURE_2D; }

static void invalidate_draw_fbos_for_texture(GLuint tex);
static void tex_destroy(Obj *o)
{
    Tex *t = (Tex *)o;
    if (t->tex) { invalidate_draw_fbos_for_texture(t->tex); invalidate_texture_bindings(); glDeleteTextures(1, &t->tex); }
    if (t->cpu) { for (int i = 0; i < t->nsub; ++i) free(t->cpu[i]); free(t->cpu); }
    free(t->map_buf);
}

static void tex_upload(Tex *t, UINT sub, UINT x, UINT y, UINT w, UINT h, const void *src, UINT pitch)
{
    UINT mip, layer; sub_info(t, sub, NULL, NULL, &mip, &layer);
    if (t->staging) {                                                        /* CPU shadow copy, D3D byte layout */
        UINT sw, sh; sub_info(t, sub, &sw, &sh, NULL, NULL);
        UINT dp = d3d_row_bytes(t->fmt, sw); uint8_t *dst = t->cpu[sub]; if (!dst) return;
        UINT bw = is_bc(t->fmt) ? 4 : 1; UINT rows = d3d_rows(t->fmt, h), bytes = d3d_row_bytes(t->fmt, w);
        for (UINT r = 0; r < rows; ++r) memcpy(dst + (size_t)(y / bw + r) * dp + d3d_row_bytes(t->fmt, x), (const uint8_t *)src + (size_t)r * pitch, bytes);
        return;
    }
    /* Formats already in GL layout can be uploaded straight from guest data.
     * GLES 3 row length handles padded streaming chunks without a repack.
     * Other formats (and pitches not expressible in pixels) retain conversion. */
    const Fmt *f = t->fmt;
    size_t row_bytes = (size_t)w * f->gl_bpp;
    int direct = f->conv == CV_NONE && f->gl_bpp == f->d3d_bpp &&
                 f->gl_bpp && pitch >= row_bytes && pitch % f->gl_bpp == 0 &&
                 pitch / f->gl_bpp <= INT32_MAX;
    /* Block-compressed Xbox textures are decoded to RGBA on drivers without
     * a native BC path. Decode large images in block-row-aligned chunks so a
     * streamed mip does not need a second full-sized RGBA allocation. */
    if (is_bc(f) && row_bytes && h && row_bytes <= SIZE_MAX / h &&
        row_bytes * h > 1024u * 1024u) {
        size_t rows = (1024u * 1024u) / row_bytes;
        if (rows < 4u) rows = 4u;
        rows &= ~(size_t)3u;
        if (rows > h) rows = h;
        if (rows > SIZE_MAX / row_bytes) return;
        uint8_t *chunk = (uint8_t *)malloc(rows * row_bytes);
        if (!chunk) return;
        bind_texture_unit(0, t->target, t->tex); activate_texture_unit(0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (UINT first = 0; first < h;) {
            UINT n = (UINT)((h - first < rows) ? h - first : rows);
            convert_to_gl(f, (const uint8_t *)src + (size_t)(first / 4u) * pitch,
                          pitch, w, n, chunk);
            glTexSubImage2D(face_target(t, layer), (GLint)mip, (GLint)x,
                            (GLint)(y + first), (GLsizei)w, (GLsizei)n,
                            f->format, f->type, chunk);
            first += n;
        }
        free(chunk); gl_check("tex_upload chunks"); return;
    }
    uint8_t *tmp = NULL;
    const void *pixels = src;
    if (!direct) {
        if (h && row_bytes > SIZE_MAX / h) return;
        size_t gsz = row_bytes * h;
        tmp = (uint8_t *)malloc(gsz ? gsz : 1); if (!tmp) return;
        convert_to_gl(f, (const uint8_t *)src, pitch, w, h, tmp);
        pixels = tmp;
    }
    bind_texture_unit(0, t->target, t->tex); activate_texture_unit(0); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    int padded = direct && pitch != row_bytes;
    if (padded) glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(pitch / f->gl_bpp));
    glTexSubImage2D(face_target(t, layer), (GLint)mip, (GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h, f->format, f->type, pixels);
    if (padded) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    free(tmp); gl_check("tex_upload");
}

static void attach_for_read(const Tex *t, UINT sub, GLuint fbo)
{
    UINT mip, layer; sub_info(t, sub, NULL, NULL, &mip, &layer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face_target(t, layer), t->tex, (GLint)mip);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
}
/* Read a region of a GPU color texture into D3D-layout bytes. */
static int tex_read(Tex *t, UINT sub, UINT x, UINT y, UINT w, UINT h, uint8_t *dst, UINT dst_pitch)
{
    if (t->fmt->depth || is_bc(t->fmt)) return 0;
    const Fmt *f = t->fmt;
    size_t row_bytes = (size_t)w * f->gl_bpp;
    int direct = f->conv == CV_NONE && f->gl_bpp == f->d3d_bpp &&
                 f->gl_bpp && dst_pitch >= row_bytes && dst_pitch % f->gl_bpp == 0 &&
                 dst_pitch / f->gl_bpp <= INT32_MAX;
    uint8_t *tmp = NULL;
    void *pixels = dst;
    if (!direct) {
        if (h && row_bytes > SIZE_MAX / h) return 0;
        size_t sz = row_bytes * h;
        tmp = (uint8_t *)malloc(sz ? sz : 1); if (!tmp) return 0;
        pixels = tmp;
    }
    attach_for_read(t, sub, G.read_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    int padded = direct && dst_pitch != row_bytes;
    if (padded) glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(dst_pitch / f->gl_bpp));
    PERF_BEGIN(read);
    glReadPixels((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h, f->format, f->type, pixels);
    PERF_END(read);
    if (padded) glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    if (tmp) { convert_from_gl(f, tmp, w, h, dst, dst_pitch); free(tmp); }
    gl_check("tex_read"); return 1;
}

HRESULT d3dgl_Device_CreateTexture2D(ID3D11Device *dev, const D3D11_TEXTURE2D_DESC *d, const D3D11_SUBRESOURCE_DATA *init, ID3D11Texture2D **out)
{
    (void)dev; if (!d || !out) return E_INVALIDARG;
    const Fmt *f = resource_fmt(d->Format, d->BindFlags);
    if (!f) { D3DGL_LOG("unsupported texture format %d", (int)d->Format); return E_INVALIDARG; }
    Tex *t = (Tex *)new_obj(sizeof(Tex), K_TEX, tex_destroy); if (!t) return E_OUTOFMEMORY;
    t->d = *d; t->fmt = f; t->cube = (d->MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0;
    t->layers = t->cube ? 6 : (d->ArraySize ? (int)d->ArraySize : 1);
    int full = 1; { UINT m = d->Width > d->Height ? d->Width : d->Height; while (m > 1) { m >>= 1; full++; } }
    t->mips = d->MipLevels ? (int)d->MipLevels : full; if (t->mips > full) t->mips = full;
    t->d.MipLevels = (UINT)t->mips; t->d.ArraySize = (UINT)t->layers;
    t->staging = d->Usage == D3D11_USAGE_STAGING; t->nsub = t->mips * t->layers; t->target = t->cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    if (t->staging) {
        t->cpu = (uint8_t **)calloc((size_t)t->nsub, sizeof(uint8_t *));
        if (!t->cpu) { d3dgl_Release(t); return E_OUTOFMEMORY; }
        for (int s = 0; s < t->nsub; ++s) {
            UINT w, h; sub_info(t, (UINT)s, &w, &h, NULL, NULL);
            size_t row = d3d_row_bytes(f, w), rows = d3d_rows(f, h);
            if (rows && row > (SIZE_MAX - 16u) / rows) { d3dgl_Release(t); return E_OUTOFMEMORY; }
            t->cpu[s] = (uint8_t *)calloc(1, row * rows + 16u);
            if (!t->cpu[s]) { d3dgl_Release(t); return E_OUTOFMEMORY; }
        }
    } else {
        glGenTextures(1, &t->tex); bind_texture_unit(0, t->target, t->tex); activate_texture_unit(0);
        glTexStorage2D(t->target, t->mips, f->internal, (GLsizei)d->Width, (GLsizei)d->Height);
        if (f->a8) { glTexParameteri(t->target, GL_TEXTURE_SWIZZLE_R, GL_ZERO); glTexParameteri(t->target, GL_TEXTURE_SWIZZLE_G, GL_ZERO);
                     glTexParameteri(t->target, GL_TEXTURE_SWIZZLE_B, GL_ZERO); glTexParameteri(t->target, GL_TEXTURE_SWIZZLE_A, GL_RED); }
        gl_check("CreateTexture2D storage");
    }
    if (init) for (int s = 0; s < t->nsub; ++s) {
        if (!init[s].pSysMem) continue; UINT w, h; sub_info(t, (UINT)s, &w, &h, NULL, NULL);
        tex_upload(t, (UINT)s, 0, 0, w, h, init[s].pSysMem, init[s].SysMemPitch ? init[s].SysMemPitch : d3d_row_bytes(f, w));
    }
    *out = (ID3D11Texture2D *)t; return S_OK;
}

void d3dgl_Texture2D_GetDesc(ID3D11Texture2D *p, D3D11_TEXTURE2D_DESC *d) { *d = ((Tex *)p)->d; }
void d3dgl_Resource_GetType(ID3D11Resource *p, D3D11_RESOURCE_DIMENSION *dim)
{ *dim = ((Obj *)p)->kind == K_BUFFER ? D3D11_RESOURCE_DIMENSION_BUFFER : D3D11_RESOURCE_DIMENSION_TEXTURE2D; }

/* ---- buffers ------------------------------------------------------------------------------------- */
static void bind_copy_write_buffer(GLuint buffer)
{
    if (!G.copy_write_valid || G.copy_write_buffer != buffer) {
        glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
        G.copy_write_buffer = buffer; G.copy_write_valid = 1;
    }
}
static void buf_destroy(Obj *o) { Buf *b = (Buf *)o; G.uniform_valid = 0;
#if defined(__ANDROID__) || defined(D3DGL_TEST_PERSISTENT)
    if (b->persistent) {
        for (unsigned i = 0; i < D3DGL_RING_GENERATIONS; ++i) {
            D3dglRingGeneration *g = &b->generations[i];
            if (g->fence) glDeleteSync(g->fence);
            if (g->buf) {
                if (g->ptr) { bind_copy_write_buffer(g->buf); glUnmapBuffer(GL_COPY_WRITE_BUFFER); }
                glDeleteBuffers(1, &g->buf);
            }
        }
        G.copy_write_valid = 0; b->buf = 0;
    }
#endif
    if (G.copy_write_valid && G.copy_write_buffer == b->buf) G.copy_write_valid = 0;
    if (b->buf) glDeleteBuffers(1, &b->buf); free(b->tmp); }
HRESULT d3dgl_Device_CreateBuffer(ID3D11Device *dev, const D3D11_BUFFER_DESC *d, const D3D11_SUBRESOURCE_DATA *init, ID3D11Buffer **out)
{
    (void)dev; if (!d || !out) return E_INVALIDARG;
    Buf *b = (Buf *)new_obj(sizeof(Buf), K_BUFFER, buf_destroy); if (!b) return E_OUTOFMEMORY;
    b->d = *d; glGenBuffers(1, &b->buf); bind_copy_write_buffer(b->buf);
    GLsizeiptr sz = (GLsizeiptr)((d->ByteWidth + 15u) & ~15u);
    glBufferData(GL_COPY_WRITE_BUFFER, sz, NULL, d->Usage == D3D11_USAGE_IMMUTABLE ? GL_STATIC_DRAW : d->Usage == D3D11_USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW);
    if (init && init->pSysMem) glBufferSubData(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)d->ByteWidth, init->pSysMem);
    gl_check("CreateBuffer"); *out = (ID3D11Buffer *)b; return S_OK;
}

/* Only the dedicated UP rings opt into immutable storage. General D3D buffers
 * still need ordinary Map/UpdateSubresource semantics. Failure leaves the old
 * mutable buffer untouched. */
#if defined(__ANDROID__) || defined(D3DGL_TEST_PERSISTENT)
typedef void (*D3dglBufferStorage)(GLenum, GLsizeiptr, const void *, GLbitfield);
#ifndef GL_MAP_PERSISTENT_BIT_EXT
#define GL_MAP_PERSISTENT_BIT_EXT 0x0040
#define GL_MAP_COHERENT_BIT_EXT 0x0080
#endif
#ifdef D3DGL_TEST_PERSISTENT
extern void d3dgl_test_buffer_storage(GLenum, GLsizeiptr, const void *, GLbitfield);
#define D3DGL_STORAGE_PROC() d3dgl_test_buffer_storage
#else
#define D3DGL_STORAGE_PROC() ((D3dglBufferStorage)eglGetProcAddress("glBufferStorageEXT"))
#endif
static int persistent_make_generation(Buf *b, D3dglRingGeneration *g, D3dglBufferStorage storage)
{
    GLuint name = 0; glGenBuffers(1, &name); if (!name) return 0;
    bind_copy_write_buffer(name);
    const GLbitfield bits = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT;
    storage(GL_COPY_WRITE_BUFFER, (GLsizeiptr)b->d.ByteWidth, NULL, bits);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteBuffers(1, &name); G.copy_write_valid = 0; return 0;
    }
    void *ptr = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)b->d.ByteWidth, bits);
    GLenum map_error = glGetError();
    if (!ptr || map_error != GL_NO_ERROR) {
        if (ptr) glUnmapBuffer(GL_COPY_WRITE_BUFFER);
        glDeleteBuffers(1, &name); G.copy_write_valid = 0; return 0;
    }
    g->buf = name; g->ptr = ptr; g->fence = NULL; return 1;
}
int d3d11gles_enable_persistent_ring(ID3D11Buffer *buffer)
{
    Buf *b = (Buf *)buffer;
    GLint count = 0; int supported = 0;
    const char *disabled = getenv("D3DGL_NO_PERSISTENT_RING");
    if ((disabled && *disabled && *disabled != '0') || !b || b->o.kind != K_BUFFER ||
        b->d.Usage != D3D11_USAGE_DYNAMIC ||
        !(b->d.BindFlags & (D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER)) || b->persistent)
        return 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; ++i) {
        const char *ext = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (ext && strcmp(ext, "GL_EXT_buffer_storage") == 0) { supported = 1; break; }
    }
    D3dglBufferStorage storage = supported ? D3DGL_STORAGE_PROC() : NULL;
    if (!storage) return 0;
    if (!persistent_make_generation(b, &b->generations[0], storage)) {
        bind_copy_write_buffer(b->buf); return 0;
    }
    glDeleteBuffers(1, &b->buf); b->buf = b->generations[0].buf;
    b->persistent = b->generations[0].ptr; b->persistent_generation = 0;
    D3DGL_LOG("UP ring: persistent coherent storage enabled (%u bytes, up to %d generations)", b->d.ByteWidth, D3DGL_RING_GENERATIONS);
#ifdef __ANDROID__
    android_trace("[gles-ring] persistent coherent storage enabled bytes=%u bind_flags=%u generations=%d", b->d.ByteWidth, b->d.BindFlags, D3DGL_RING_GENERATIONS);
#endif
    return 1;
}
/* Called only on the GL owner, at a ring discard. A newly inserted fence must
 * replace any older fence for the active generation: a failed earlier wrap
 * can be followed by more draws that read the same storage. */
static int persistent_rotate(Buf *b)
{
    D3dglRingGeneration *old = &b->generations[b->persistent_generation];
    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (!fence) return 0;
    if (old->fence) glDeleteSync(old->fence);
    old->fence = fence;
    int next = (b->persistent_generation + 1) % D3DGL_RING_GENERATIONS;
    D3dglRingGeneration *g = &b->generations[next];
    if (!g->buf) {
        D3dglBufferStorage storage = D3DGL_STORAGE_PROC();
        if (!storage || !persistent_make_generation(b, g, storage)) {
            /* Allocation failed: wait for the current generation before
             * reusing it, rather than overwriting GPU-visible bytes. */
            g = old; next = b->persistent_generation;
        }
    }
    if (g->fence) {
        GLenum result;
        do { result = glClientWaitSync(g->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL); }
        while (result == GL_TIMEOUT_EXPIRED);
        if (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) return 0;
        glDeleteSync(g->fence); g->fence = NULL;
    }
    b->persistent_generation = next; b->buf = g->buf; b->persistent = g->ptr;
    return 1;
}
#else
int d3d11gles_enable_persistent_ring(ID3D11Buffer *buffer) { (void)buffer; return 0; }
#endif

/* Map only the written range. On wrap, discard old storage so queued draws
 * retain their data; disjoint ranges use NO_OVERWRITE. */
static HRESULT upload_transient_range(ID3D11Buffer *buffer, UINT offset,
                                      const void *data, UINT size, BOOL discard, UINT bind_flag)
{
    Buf *b = (Buf *)buffer;
    if (!b || b->o.kind != K_BUFFER || !data || !size ||
        b->d.Usage != D3D11_USAGE_DYNAMIC || !(b->d.BindFlags & bind_flag) ||
        offset > b->d.ByteWidth || size > b->d.ByteWidth - offset || b->mapped_gl || b->tmp)
        return E_INVALIDARG;
#if defined(__ANDROID__) || defined(D3DGL_TEST_PERSISTENT)
    if (b->persistent) {
        /* The fence covers all earlier draws, including cache hits. Do not
         * overwrite any old range until the GPU has finished the entire ring. */
        if (discard && b->persistent_written) {
#ifdef __ANDROID__
            double wait_start = perf_now();
            ++perf_workload.persistent_wraps;
#endif
            int rotated = persistent_rotate(b);
#ifdef __ANDROID__
            double wait_end = perf_now();
            if (wait_start > 0.0 && wait_end >= wait_start) perf_workload.persistent_wait_seconds += wait_end - wait_start;
#endif
            if (!rotated) return E_FAIL;
        }
        memcpy(b->persistent + offset, data, size);
        b->persistent_written = 1;
#ifdef __ANDROID__
        ++perf_workload.persistent_uploads;
#endif
        return S_OK;
    }
#endif
    bind_copy_write_buffer(b->buf);
    GLbitfield flags = GL_MAP_WRITE_BIT | (discard ? GL_MAP_INVALIDATE_BUFFER_BIT : GL_MAP_UNSYNCHRONIZED_BIT);
    PERF_BEGIN(ringmap);
    void *dst = glMapBufferRange(GL_COPY_WRITE_BUFFER, (GLintptr)offset, (GLsizeiptr)size, flags);
    PERF_END(ringmap);
    if (dst) {
        memcpy(dst, data, size);
        { PERF_BEGIN(ringunmap); GLboolean ok = glUnmapBuffer(GL_COPY_WRITE_BUFFER); PERF_END(ringunmap);
          if (!ok) return E_FAIL; }
    } else {
        /* Preserve the DISCARD/NO_OVERWRITE contracts even if mapping fails. */
        if (discard) glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)b->d.ByteWidth, NULL, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)offset, (GLsizeiptr)size, data);
    }
    gl_check_hot("upload_vertex_range");
    return S_OK;
}

HRESULT d3d11gles_upload_vertex_range(ID3D11Buffer *buffer, UINT offset,
                                      const void *data, UINT size, BOOL discard)
{ return upload_transient_range(buffer, offset, data, size, discard, D3D11_BIND_VERTEX_BUFFER); }
HRESULT d3d11gles_upload_index_range(ID3D11Buffer *buffer, UINT offset,
                                     const void *data, UINT size, BOOL discard)
{ return upload_transient_range(buffer, offset, data, size, discard, D3D11_BIND_INDEX_BUFFER); }

/* ---- Map / Unmap / UpdateSubresource ------------------------------------------------------------------ */
HRESULT d3dgl_Ctx_Map(ID3D11DeviceContext *c, ID3D11Resource *res, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *m)
{
    (void)c; (void)flags; Obj *o = (Obj *)res; if (!o || !m) return E_INVALIDARG;
    if (o->kind == K_BUFFER) {
        Buf *b = (Buf *)o; GLbitfield acc = 0;
        /* These rings are already persistently mapped. General Map/Unmap is
         * not a supported access path and mapping them again is invalid GLES. */
        if (b->persistent) return E_INVALIDARG;
        switch (type) { case D3D11_MAP_READ: acc = GL_MAP_READ_BIT; break; case D3D11_MAP_READ_WRITE: acc = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; break;
                        case D3D11_MAP_WRITE_DISCARD: acc = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT; break;
                        case D3D11_MAP_WRITE_NO_OVERWRITE: acc = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT; break; default: acc = GL_MAP_WRITE_BIT; }
        bind_copy_write_buffer(b->buf);
        PERF_BEGIN(map);
        void *p = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)b->d.ByteWidth, acc);
        PERF_END(map);
        b->map_type = type;
        if (p) b->mapped_gl = 1;
        else { b->mapped_gl = 0; free(b->tmp); b->tmp = (uint8_t *)calloc(1, b->d.ByteWidth); p = b->tmp; }   /* fallback: shadow + upload */
        m->pData = p; m->RowPitch = b->d.ByteWidth; m->DepthPitch = b->d.ByteWidth; return p ? S_OK : E_FAIL;
    }
    Tex *t = (Tex *)o; if (sub >= (UINT)t->nsub) return E_INVALIDARG;
    UINT w, h; sub_info(t, sub, &w, &h, NULL, NULL); UINT pitch = d3d_row_bytes(t->fmt, w);
    t->map_sub = (int)sub; t->map_type = type;
    if (t->staging) { m->pData = t->cpu[sub]; m->RowPitch = pitch; m->DepthPitch = pitch * d3d_rows(t->fmt, h); return S_OK; }
    free(t->map_buf); t->map_buf = (uint8_t *)calloc(1, (size_t)pitch * d3d_rows(t->fmt, h) + 16);
    if (!t->map_buf) return E_OUTOFMEMORY;
    if (type == D3D11_MAP_READ || type == D3D11_MAP_READ_WRITE) tex_read(t, sub, 0, 0, w, h, t->map_buf, pitch);
    m->pData = t->map_buf; m->RowPitch = pitch; m->DepthPitch = pitch * d3d_rows(t->fmt, h); return S_OK;
}
void d3dgl_Ctx_Unmap(ID3D11DeviceContext *c, ID3D11Resource *res, UINT sub)
{
    (void)c; (void)sub; Obj *o = (Obj *)res; if (!o) return;
    if (o->kind == K_BUFFER) {
        Buf *b = (Buf *)o; bind_copy_write_buffer(b->buf);
        if (b->mapped_gl) { PERF_BEGIN(unmap); glUnmapBuffer(GL_COPY_WRITE_BUFFER); PERF_END(unmap); b->mapped_gl = 0; }
        else if (b->tmp) { if (b->map_type != D3D11_MAP_READ) glBufferSubData(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)b->d.ByteWidth, b->tmp); free(b->tmp); b->tmp = NULL; }
        gl_check_hot("Unmap(buffer)"); return;
    }
    Tex *t = (Tex *)o;
    if (!t->staging && t->map_buf && t->map_type != D3D11_MAP_READ) {
        UINT w, h; sub_info(t, (UINT)t->map_sub, &w, &h, NULL, NULL);
        tex_upload(t, (UINT)t->map_sub, 0, 0, w, h, t->map_buf, d3d_row_bytes(t->fmt, w));
    }
    if (!t->staging) { free(t->map_buf); t->map_buf = NULL; }
}
void d3dgl_Ctx_UpdateSubresource(ID3D11DeviceContext *c, ID3D11Resource *res, UINT sub, const D3D11_BOX *box, const void *data, UINT rp, UINT dp)
{
    (void)c; (void)dp; Obj *o = (Obj *)res; if (!o || !data) return;
    if (o->kind == K_BUFFER) {
        Buf *b = (Buf *)o; UINT off = box ? box->left : 0, n = box ? box->right - box->left : b->d.ByteWidth;
        bind_copy_write_buffer(b->buf); glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)off, (GLsizeiptr)n, data); gl_check("UpdateSubresource(buffer)"); return;
    }
    Tex *t = (Tex *)o; UINT w, h; sub_info(t, sub, &w, &h, NULL, NULL);
    UINT x = box ? box->left : 0, y = box ? box->top : 0, bw = box ? box->right - box->left : w, bh = box ? box->bottom - box->top : h;
    tex_upload(t, sub, x, y, bw, bh, data, rp ? rp : d3d_row_bytes(t->fmt, bw));
}
