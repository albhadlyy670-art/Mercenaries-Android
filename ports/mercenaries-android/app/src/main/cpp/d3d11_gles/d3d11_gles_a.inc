/* ===== part A: objects, formats, conversions, textures, buffers =========== */
#include "d3d11_gles_types.h"
#include "gles_gl.h"
#include "hlsl2glsl.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#if defined(__ANDROID__)
#include <android/log.h>
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
#define SETREF(dst, src) do { void *_n = (src); if (_n) d3dgl_AddRef(_n); void *_o = (dst); (dst) = _n; if (_o) d3dgl_Release(_o); } while (0)

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
    Obj o; D3D11_TEXTURE2D_DESC d; GLuint tex; GLenum target; const Fmt *fmt; int cube, mips, layers, staging;
    uint8_t **cpu; int nsub; int map_sub; uint8_t *map_buf; D3D11_MAP map_type;
};
struct Buf { Obj o; D3D11_BUFFER_DESC d; GLuint buf; uint8_t *tmp; D3D11_MAP map_type; int mapped_gl; };
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

typedef struct { Shader *vs, *ps; Program *p; } ProgKey;
struct Program { GLuint prog; Shader *vs, *ps; int n_vs, n_ps; uint32_t bgra; int ps_unit0; };

typedef struct {
    int inited; Ctx *ctx; D3d11GlesHost host; int dbg;
    GLuint vao, fbo, read_fbo, draw_fbo; GLuint att_tex[MAX_RT + 1], att_target[MAX_RT + 1]; GLint att_level[MAX_RT + 1]; int fbo_dirty;
    ProgKey progs[MAX_PROGRAMS]; int nprogs; int errors; int max_units;
} Global;
static Global G;

void d3d11gles_set_host(const D3d11GlesHost *h) { if (h) G.host = *h; else memset(&G.host, 0, sizeof(G.host)); }
int d3d11gles_take_error_count(void) { int e = G.errors; G.errors = 0; return e; }
static void gl_check(const char *where)
{
    GLenum e; int n = 0;
    while ((e = glGetError()) != GL_NO_ERROR && n++ < 8) { G.errors++; if (G.dbg) D3DGL_LOG("GL error 0x%04X after %s", e, where); }
}

/* ---- textures ---------------------------------------------------------------------------------- */
static UINT dimu(UINT v, UINT m) { v >>= m; return v ? v : 1; }
static void sub_info(const Tex *t, UINT sub, UINT *w, UINT *h, UINT *mip, UINT *layer)
{
    UINT m = sub % (UINT)t->mips, l = sub / (UINT)t->mips;
    if (w) *w = dimu(t->d.Width, m); if (h) *h = dimu(t->d.Height, m); if (mip) *mip = m; if (layer) *layer = l;
}
static GLenum face_target(const Tex *t, UINT layer) { return t->cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer : GL_TEXTURE_2D; }

static void tex_destroy(Obj *o)
{
    Tex *t = (Tex *)o;
    if (t->tex) glDeleteTextures(1, &t->tex);
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
    size_t gsz = (size_t)w * h * t->fmt->gl_bpp; uint8_t *tmp = (uint8_t *)malloc(gsz ? gsz : 1); if (!tmp) return;
    convert_to_gl(t->fmt, (const uint8_t *)src, pitch, w, h, tmp);
    glActiveTexture(GL_TEXTURE0); glBindTexture(t->target, t->tex); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(face_target(t, layer), (GLint)mip, (GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h, t->fmt->format, t->fmt->type, tmp);
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
    size_t sz = (size_t)w * h * t->fmt->gl_bpp; uint8_t *tmp = (uint8_t *)malloc(sz ? sz : 1); if (!tmp) return 0;
    attach_for_read(t, sub, G.read_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h, t->fmt->format, t->fmt->type, tmp);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    convert_from_gl(t->fmt, tmp, w, h, dst, dst_pitch); free(tmp); gl_check("tex_read"); return 1;
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
        for (int s = 0; s < t->nsub; ++s) { UINT w, h; sub_info(t, (UINT)s, &w, &h, NULL, NULL); t->cpu[s] = (uint8_t *)calloc(1, (size_t)d3d_row_bytes(f, w) * d3d_rows(f, h) + 16); }
    } else {
        glGenTextures(1, &t->tex); glActiveTexture(GL_TEXTURE0); glBindTexture(t->target, t->tex);
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
static void buf_destroy(Obj *o) { Buf *b = (Buf *)o; if (b->buf) glDeleteBuffers(1, &b->buf); free(b->tmp); }
HRESULT d3dgl_Device_CreateBuffer(ID3D11Device *dev, const D3D11_BUFFER_DESC *d, const D3D11_SUBRESOURCE_DATA *init, ID3D11Buffer **out)
{
    (void)dev; if (!d || !out) return E_INVALIDARG;
    Buf *b = (Buf *)new_obj(sizeof(Buf), K_BUFFER, buf_destroy); if (!b) return E_OUTOFMEMORY;
    b->d = *d; glGenBuffers(1, &b->buf); glBindBuffer(GL_COPY_WRITE_BUFFER, b->buf);
    GLsizeiptr sz = (GLsizeiptr)((d->ByteWidth + 15u) & ~15u);
    glBufferData(GL_COPY_WRITE_BUFFER, sz, NULL, d->Usage == D3D11_USAGE_IMMUTABLE ? GL_STATIC_DRAW : d->Usage == D3D11_USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW);
    if (init && init->pSysMem) glBufferSubData(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)d->ByteWidth, init->pSysMem);
    gl_check("CreateBuffer"); *out = (ID3D11Buffer *)b; return S_OK;
}

/* ---- Map / Unmap / UpdateSubresource ------------------------------------------------------------------ */
HRESULT d3dgl_Ctx_Map(ID3D11DeviceContext *c, ID3D11Resource *res, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *m)
{
    (void)c; (void)flags; Obj *o = (Obj *)res; if (!o || !m) return E_INVALIDARG;
    if (o->kind == K_BUFFER) {
        Buf *b = (Buf *)o; GLbitfield acc = 0;
        switch (type) { case D3D11_MAP_READ: acc = GL_MAP_READ_BIT; break; case D3D11_MAP_READ_WRITE: acc = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; break;
                        case D3D11_MAP_WRITE_DISCARD: acc = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT; break;
                        case D3D11_MAP_WRITE_NO_OVERWRITE: acc = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT; break; default: acc = GL_MAP_WRITE_BIT; }
        glBindBuffer(GL_COPY_WRITE_BUFFER, b->buf);
        void *p = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)b->d.ByteWidth, acc);
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
        Buf *b = (Buf *)o; glBindBuffer(GL_COPY_WRITE_BUFFER, b->buf);
        if (b->mapped_gl) { glUnmapBuffer(GL_COPY_WRITE_BUFFER); b->mapped_gl = 0; }
        else if (b->tmp) { if (b->map_type != D3D11_MAP_READ) glBufferSubData(GL_COPY_WRITE_BUFFER, 0, (GLsizeiptr)b->d.ByteWidth, b->tmp); free(b->tmp); b->tmp = NULL; }
        gl_check("Unmap(buffer)"); return;
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
        glBindBuffer(GL_COPY_WRITE_BUFFER, b->buf); glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)off, (GLsizeiptr)n, data); gl_check("UpdateSubresource(buffer)"); return;
    }
    Tex *t = (Tex *)o; UINT w, h; sub_info(t, sub, &w, &h, NULL, NULL);
    UINT x = box ? box->left : 0, y = box ? box->top : 0, bw = box ? box->right - box->left : w, bh = box ? box->bottom - box->top : h;
    tex_upload(t, sub, x, y, bw, bh, data, rp ? rp : d3d_row_bytes(t->fmt, bw));
}
