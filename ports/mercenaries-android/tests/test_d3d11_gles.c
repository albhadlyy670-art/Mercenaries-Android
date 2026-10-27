/* Drives d3d11_gles purely through D3D11 API calls and checks rendered pixels on a real GLES 3 driver. */
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void *EGLDisplay,*EGLConfig,*EGLContext,*EGLSurface; typedef int32_t EGLint; typedef unsigned EGLBoolean,EGLenum;
EGLDisplay eglGetPlatformDisplay(EGLenum,void*,const EGLint*); EGLBoolean eglInitialize(EGLDisplay,EGLint*,EGLint*); EGLBoolean eglBindAPI(EGLenum);
EGLBoolean eglChooseConfig(EGLDisplay,const EGLint*,EGLConfig*,EGLint,EGLint*); EGLContext eglCreateContext(EGLDisplay,EGLConfig,EGLContext,const EGLint*);
EGLBoolean eglMakeCurrent(EGLDisplay,EGLSurface,EGLSurface,EGLContext);

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static ID3D11Device *dev; static ID3D11DeviceContext *ctx; static IDXGISwapChain *sc; static int presents;
static void on_present(void *u, uint32_t t, int w, int h, int s) { (void)u; (void)t; (void)w; (void)h; (void)s; presents++; }

static ID3D10Blob *compile(const char *src, const char *entry, const char *target)
{
    ID3D10Blob *code = NULL, *err = NULL;
    HRESULT hr = D3DCompile(src, strlen(src), "test", NULL, NULL, entry, target, 0, 0, &code, &err);
    if (FAILED(hr)) { printf("  compile %s failed: %s\n", target, err ? (char *)ID3D10Blob_GetBufferPointer(err) : "?"); fails++; return NULL; }
    return code;
}
static ID3D11Texture2D *make_tex(UINT w, UINT h, DXGI_FORMAT f, UINT bind, D3D11_USAGE u, const void *data, UINT pitch)
{
    D3D11_TEXTURE2D_DESC d = { w, h, 1, 1, f, { 1, 0 }, u, bind, u == D3D11_USAGE_STAGING ? D3D11_CPU_ACCESS_READ : 0, 0 };
    D3D11_SUBRESOURCE_DATA sd = { data, pitch, 0 }; ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(dev, &d, data ? &sd : NULL, &t); if (FAILED(hr)) { printf("  CreateTexture2D failed\n"); fails++; } return t;
}
/* Read an RT back through a staging texture. Returns D3D-order bytes (row 0 = top). */
static uint8_t *readback(ID3D11Texture2D *rt, UINT w, UINT h, DXGI_FORMAT f, UINT *pitch)
{
    ID3D11Texture2D *st = make_tex(w, h, f, 0, D3D11_USAGE_STAGING, NULL, 0); D3D11_MAPPED_SUBRESOURCE m;
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)st, (ID3D11Resource *)rt);
    if (FAILED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m))) { fails++; return NULL; }
    uint8_t *out = malloc((size_t)m.RowPitch * h); memcpy(out, m.pData, (size_t)m.RowPitch * h); *pitch = m.RowPitch;
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0); ID3D11Texture2D_Release(st); return out;
}
#define PX(buf, pitch, x, y) ((buf) + (size_t)(y) * (pitch) + (x) * 4)
static int near_px(const uint8_t *p, int r, int g, int b, int a) { return abs(p[0]-r) <= 2 && abs(p[1]-g) <= 2 && abs(p[2]-b) <= 2 && abs(p[3]-a) <= 2; }

typedef struct { float x, y, z; float r, g, b, a; float u, v; } Vtx;
static ID3D11Buffer *make_buf(const void *data, UINT size, UINT bind, D3D11_USAGE u)
{
    D3D11_BUFFER_DESC d = { size, u, bind, u == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0, 0, 0 }; D3D11_SUBRESOURCE_DATA sd = { data, 0, 0 }; ID3D11Buffer *b = NULL;
    if (FAILED(ID3D11Device_CreateBuffer(dev, &d, data ? &sd : NULL, &b))) { printf("  CreateBuffer failed\n"); fails++; } return b;
}

static const char *HLSL_VS =
 "cbuffer Frame : register(b0) { float4 Tint; float4x4 Xform; };\n"
 "struct VSIn { float3 pos : POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };\n"
 "struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };\n"
 "VSOut main(VSIn i) { VSOut o; o.pos = mul(float4(i.pos, 1.0), Xform); o.col = i.col * Tint; o.uv = i.uv; return o; }\n";
static const char *HLSL_PS_COLOR =
 "struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };\n"
 "float4 main(PSIn i) : SV_Target { return i.col; }\n";
static const char *HLSL_PS_TEX =
 "Texture2D tex0 : register(t0); SamplerState smp0 : register(s0);\n"
 "struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };\n"
 "float4 main(PSIn i) : SV_Target { return tex0.Sample(smp0, i.uv); }\n";
static const char *HLSL_PS_EXTRA = /* reads TEXCOORD1, which the VS never writes */
 "struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; float4 extra : TEXCOORD1; };\n"
 "float4 main(PSIn i) : SV_Target { return i.col + i.extra; }\n";

int main(void)
{
    EGLDisplay d = eglGetPlatformDisplay(0x31DD, 0, 0); eglInitialize(d, 0, 0); eglBindAPI(0x30A0);
    EGLint a[] = { 0x3040, 0x40, 0x3033, 1, 0x3038 }; EGLConfig c; EGLint n; eglChooseConfig(d, a, &c, 1, &n);
    EGLint ca[] = { 0x3098, 3, 0x3038 }; EGLContext x = eglCreateContext(d, c, 0, ca); eglMakeCurrent(d, 0, 0, x);
    D3d11GlesHost host = { on_present, NULL }; d3d11gles_set_host(&host);

    DXGI_SWAP_CHAIN_DESC scd; memset(&scd, 0, sizeof(scd)); scd.BufferDesc.Width = 64; scd.BufferDesc.Height = 64; scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; scd.BufferCount = 1;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &scd, &sc, &dev, &fl, &ctx);
    CHECK(SUCCEEDED(hr), "device creation"); if (FAILED(hr)) return 1;

    ID3D11Texture2D *bb = NULL; IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    ID3D11RenderTargetView *rtv = NULL; ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
    D3D11_VIEWPORT vp = { 0, 0, 64, 64, 0, 1 }; ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);

    ID3D10Blob *vsb = compile(HLSL_VS, "main", "vs_5_0"), *psb = compile(HLSL_PS_COLOR, "main", "ps_5_0"), *pstb = compile(HLSL_PS_TEX, "main", "ps_5_0"), *psx = compile(HLSL_PS_EXTRA, "main", "ps_5_0");
    ID3D11VertexShader *vs; ID3D11PixelShader *ps, *pst, *psex;
    ID3D11Device_CreateVertexShader(dev, ID3D10Blob_GetBufferPointer(vsb), ID3D10Blob_GetBufferSize(vsb), NULL, &vs);
    ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(psb), ID3D10Blob_GetBufferSize(psb), NULL, &ps);
    ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(pstb), ID3D10Blob_GetBufferSize(pstb), NULL, &pst);
    ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(psx), ID3D10Blob_GetBufferSize(psx), NULL, &psex);
    D3D11_INPUT_ELEMENT_DESC el[3] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                                       { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                                       { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *lay; hr = ID3D11Device_CreateInputLayout(dev, el, 3, ID3D10Blob_GetBufferPointer(vsb), ID3D10Blob_GetBufferSize(vsb), &lay);
    CHECK(SUCCEEDED(hr), "CreateInputLayout");

    struct { float tint[4]; float m[16]; } cb = { { 1, 1, 1, 1 }, { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } };   /* identity (column-major packing, symmetric) */
    ID3D11Buffer *cbuf = make_buf(&cb, sizeof(cb), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
    D3D11_RASTERIZER_DESC rd = { D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, 0, 0, 0, TRUE, FALSE, FALSE, FALSE }; ID3D11RasterizerState *rs_back; ID3D11Device_CreateRasterizerState(dev, &rd, &rs_back);
    rd.CullMode = D3D11_CULL_NONE; ID3D11RasterizerState *rs_none; ID3D11Device_CreateRasterizerState(dev, &rd, &rs_none);
    rd.ScissorEnable = TRUE; ID3D11RasterizerState *rs_sc; ID3D11Device_CreateRasterizerState(dev, &rd, &rs_sc);
    D3D11_SAMPLER_DESC sdsc = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1, D3D11_COMPARISON_NEVER, { 0,0,0,0 }, 0, D3D11_FLOAT32_MAX };
    ID3D11SamplerState *smp; ID3D11Device_CreateSamplerState(dev, &sdsc, &smp);
    D3D11_DEPTH_STENCIL_DESC dsd; memset(&dsd, 0, sizeof(dsd)); dsd.DepthEnable = FALSE; ID3D11DepthStencilState *ds_off; ID3D11Device_CreateDepthStencilState(dev, &dsd, &ds_off);
    dsd.DepthEnable = TRUE; dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dsd.DepthFunc = D3D11_COMPARISON_LESS; ID3D11DepthStencilState *ds_on; ID3D11Device_CreateDepthStencilState(dev, &dsd, &ds_on);
    D3D11_BLEND_DESC bd; memset(&bd, 0, sizeof(bd)); bd.RenderTarget[0].RenderTargetWriteMask = 15; ID3D11BlendState *bs_off; ID3D11Device_CreateBlendState(dev, &bd, &bs_off);
    bd.RenderTarget[0].BlendEnable = TRUE; bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; ID3D11BlendState *bs_alpha; ID3D11Device_CreateBlendState(dev, &bd, &bs_alpha);

    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL); ID3D11DeviceContext_IASetInputLayout(ctx, lay);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(ctx, vs, NULL, 0); ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &cbuf); ID3D11DeviceContext_RSSetState(ctx, rs_back);
    ID3D11DeviceContext_OMSetDepthStencilState(ctx, ds_off, 0); ID3D11DeviceContext_OMSetBlendState(ctx, bs_off, NULL, 0xFFFFFFFFu);
    UINT stride = sizeof(Vtx), off = 0; float clear[4] = { 0, 0, 1, 1 };
    uint8_t *px; UINT pitch;

    printf("[1] clear\n");
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch);
    CHECK(near_px(PX(px, pitch, 5, 5), 0, 0, 255, 255) || near_px(PX(px, pitch, 5, 5), 255, 0, 0, 255), "cleared pixel is blue in BGRA layout (got %d %d %d %d)", PX(px,pitch,5,5)[0], PX(px,pitch,5,5)[1], PX(px,pitch,5,5)[2], PX(px,pitch,5,5)[3]);
    CHECK(PX(px, pitch, 5, 5)[0] == 255 && PX(px, pitch, 5, 5)[2] == 0, "BGRA read-back order: byte0 = blue");
    free(px);

    printf("[2] vertical orientation: quad in the TOP half (NDC y 0..1) must land in rows 0..31\n");
    Vtx top[6] = { { -1, 1, .5f, 0,1,0,1, 0,0 }, { 1, 1, .5f, 0,1,0,1, 1,0 }, { -1, 0, .5f, 0,1,0,1, 0,1 },
                   { -1, 0, .5f, 0,1,0,1, 0,1 }, { 1, 1, .5f, 0,1,0,1, 1,0 }, { 1, 0, .5f, 0,1,0,1, 1,1 } };
    ID3D11Buffer *vbt = make_buf(top, sizeof(top), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT);
    ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vbt, &stride, &off);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_Draw(ctx, 6, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch);
    CHECK(PX(px, pitch, 32, 10)[1] == 255 && PX(px, pitch, 32, 10)[0] == 0, "row 10 (top half) is green");
    CHECK(PX(px, pitch, 32, 50)[0] == 255 && PX(px, pitch, 32, 50)[1] == 0, "row 50 (bottom half) stays blue (clear colour)");
    free(px);

    printf("[3] winding: CW triangle visible, CCW culled (CullMode=BACK)\n");
    Vtx cw[3] = { { -1,-1,.5f, 1,0,0,1,0,0 }, { 0,1,.5f, 1,0,0,1,0,0 }, { 1,-1,.5f, 1,0,0,1,0,0 } };
    Vtx ccw[3] = { cw[0], cw[2], cw[1] };
    ID3D11Buffer *vcw = make_buf(cw, sizeof(cw), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT), *vccw = make_buf(ccw, sizeof(ccw), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vcw, &stride, &off); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px, pitch, 32, 55)[2] == 255, "CW triangle drawn (red at 32,55)"); free(px);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vccw, &stride, &off); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px, pitch, 32, 55)[2] == 0, "CCW triangle culled"); free(px);
    ID3D11DeviceContext_RSSetState(ctx, rs_none); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px, pitch, 32, 55)[2] == 255, "CCW visible with CullMode=NONE"); free(px);

    printf("[4] texture sampling orientation (RGBA 2x2) on a 2x2 render target\n");
    uint8_t texels[16] = { 255,0,0,255,  0,255,0,255,   0,0,255,255,  255,255,0,255 };   /* TL red, TR green / BL blue, BR yellow */
    ID3D11Texture2D *t2 = make_tex(2, 2, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, texels, 8);
    ID3D11ShaderResourceView *srv; ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)t2, NULL, &srv);
    ID3D11Texture2D *rt2 = make_tex(2, 2, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, NULL, 0);
    ID3D11RenderTargetView *rtv2; ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)rt2, NULL, &rtv2);
    Vtx quad[6] = { { -1, 1, .5f, 1,1,1,1, 0,0 }, { 1, 1, .5f, 1,1,1,1, 1,0 }, { -1,-1, .5f, 1,1,1,1, 0,1 }, { -1,-1,.5f, 1,1,1,1, 0,1 }, { 1, 1,.5f, 1,1,1,1, 1,0 }, { 1,-1,.5f, 1,1,1,1, 1,1 } };
    ID3D11Buffer *vq = make_buf(quad, sizeof(quad), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT);
    D3D11_VIEWPORT vp2 = { 0, 0, 2, 2, 0, 1 }; ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp2);
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv2, NULL); ID3D11DeviceContext_PSSetShader(ctx, pst, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv); ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &smp);
    ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vq, &stride, &off); ID3D11DeviceContext_Draw(ctx, 6, 0);
    px = readback(rt2, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM, &pitch);
    CHECK(near_px(PX(px,pitch,0,0),255,0,0,255), "texel (0,0) red");   CHECK(near_px(PX(px,pitch,1,0),0,255,0,255), "texel (1,0) green");
    CHECK(near_px(PX(px,pitch,0,1),0,0,255,255), "texel (0,1) blue");  CHECK(near_px(PX(px,pitch,1,1),255,255,0,255), "texel (1,1) yellow"); free(px);

    printf("[5] BGRA texture upload is logically RGBA\n");
    uint8_t bgra[16]; for (int i = 0; i < 4; ++i) { bgra[4*i] = texels[4*i+2]; bgra[4*i+1] = texels[4*i+1]; bgra[4*i+2] = texels[4*i]; bgra[4*i+3] = texels[4*i+3]; }
    ID3D11Texture2D *tb = make_tex(2, 2, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, bgra, 8); ID3D11ShaderResourceView *srvb; ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tb, NULL, &srvb);
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srvb); ID3D11DeviceContext_Draw(ctx, 6, 0);
    px = readback(rt2, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM, &pitch); CHECK(near_px(PX(px,pitch,0,0),255,0,0,255) && near_px(PX(px,pitch,1,1),255,255,0,255), "BGRA texels sample as the same colours"); free(px);

    printf("[6] BC1 decode\n");
    uint8_t blk[8] = { 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };           /* c0 = pure red (F800), c1 = pure blue (001F), all indices 0 -> red */
    uint8_t blk2[8]; memcpy(blk2, blk, 8); blk2[4] = 0x55; blk2[5] = 0x55; blk2[6] = 0x55; blk2[7] = 0x55;   /* all indices 1 -> blue */
    uint8_t bc_all[8]; memcpy(bc_all, blk, 8);
    D3D11_TEXTURE2D_DESC bcd = { 4, 4, 1, 1, DXGI_FORMAT_BC1_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 }; D3D11_SUBRESOURCE_DATA bsd = { bc_all, 8, 0 };
    ID3D11Texture2D *tbc = NULL; hr = ID3D11Device_CreateTexture2D(dev, &bcd, &bsd, &tbc); CHECK(SUCCEEDED(hr), "BC1 texture creation");
    ID3D11ShaderResourceView *srvc; ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tbc, NULL, &srvc);
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srvc); ID3D11DeviceContext_Draw(ctx, 6, 0);
    px = readback(rt2, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM, &pitch); CHECK(near_px(PX(px,pitch,0,0),255,0,0,255), "BC1 block decodes to red (got %d %d %d)", PX(px,pitch,0,0)[0], PX(px,pitch,0,0)[1], PX(px,pitch,0,0)[2]); free(px);
    ID3D11ShaderResourceView *nullsrv = NULL; ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &nullsrv);

    ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp); ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL); ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);

    printf("[7] depth test (draw far then near, and near then far)\n");
    ID3D11Texture2D *dtex = make_tex(64, 64, DXGI_FORMAT_R24G8_TYPELESS, D3D11_BIND_DEPTH_STENCIL, D3D11_USAGE_DEFAULT, NULL, 0); ID3D11DepthStencilView *dsv; D3D11_DEPTH_STENCIL_VIEW_DESC dvd = { DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_DSV_DIMENSION_TEXTURE2D, 0, { 0 } };
    ID3D11Device_CreateDepthStencilView(dev, (ID3D11Resource *)dtex, &dvd, &dsv);
    Vtx farq[3] = { { -1,-1,.8f, 1,0,0,1,0,0 }, { 0,1,.8f, 1,0,0,1,0,0 }, { 1,-1,.8f, 1,0,0,1,0,0 } }, nearq[3] = { { -1,-1,.2f, 0,1,0,1,0,0 }, { 0,1,.2f, 0,1,0,1,0,0 }, { 1,-1,.2f, 0,1,0,1,0,0 } };
    ID3D11Buffer *vfar = make_buf(farq, sizeof(farq), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT), *vnear = make_buf(nearq, sizeof(nearq), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT);
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, dsv); ID3D11DeviceContext_OMSetDepthStencilState(ctx, ds_on, 0);
    for (int order = 0; order < 2; ++order) {
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_ClearDepthStencilView(ctx, dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        ID3D11Buffer *first = order ? vnear : vfar, *second = order ? vfar : vnear;
        ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &first, &stride, &off); ID3D11DeviceContext_Draw(ctx, 3, 0);
        ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &second, &stride, &off); ID3D11DeviceContext_Draw(ctx, 3, 0);
        px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px, pitch, 32, 55)[1] == 255 && PX(px, pitch, 32, 55)[2] == 0, "nearer (green) triangle wins, draw order %d", order); free(px);
    }
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL); ID3D11DeviceContext_OMSetDepthStencilState(ctx, ds_off, 0);

    printf("[8] alpha blend\n");
    Vtx half[3] = { { -1,-1,.5f, 1,0,0,.5f,0,0 }, { 0,3,.5f, 1,0,0,.5f,0,0 }, { 3,-1,.5f, 1,0,0,.5f,0,0 } };   /* covers the whole target */
    ID3D11Buffer *vh = make_buf(half, sizeof(half), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT); ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vh, &stride, &off);
    ID3D11DeviceContext_OMSetBlendState(ctx, bs_alpha, NULL, 0xFFFFFFFFu); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(abs(PX(px,pitch,32,32)[2] - 128) <= 3 && abs(PX(px,pitch,32,32)[0] - 128) <= 3, "50%% red over blue = purple (got r=%d b=%d)", PX(px,pitch,32,32)[2], PX(px,pitch,32,32)[0]); free(px);
    ID3D11DeviceContext_OMSetBlendState(ctx, bs_off, NULL, 0xFFFFFFFFu);

    printf("[9] constant buffer: UpdateSubresource and Map(WRITE_DISCARD)\n");
    ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vh, &stride, &off);
    cb.tint[0] = 0; cb.tint[1] = 1; cb.tint[2] = 0; cb.tint[3] = 1; ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource *)cbuf, 0, NULL, &cb, 0, 0);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px,pitch,32,32)[1] == 0 && PX(px,pitch,32,32)[2] == 0, "tint (0,1,0) * red vertex = black"); free(px);
    ID3D11Buffer *dyn = make_buf(&cb, sizeof(cb), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC); D3D11_MAPPED_SUBRESOURCE mp;
    CHECK(SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)dyn, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp)), "Map dynamic cbuffer");
    float *f = (float *)mp.pData; memcpy(f, &cb, sizeof(cb)); f[0] = 1; f[1] = 1; f[2] = 1; f[3] = 1; ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)dyn, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &dyn); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px,pitch,32,32)[2] == 255 || PX(px,pitch,32,32)[2] >= 120, "mapped cbuffer update takes effect (red=%d)", PX(px,pitch,32,32)[2]); free(px);
    ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &cbuf);

    printf("[10] indexed draw with base vertex\n");
    Vtx iv[6] = { { 9,9,0,0,0,0,0,0,0 }, { 9,9,0,0,0,0,0,0,0 }, { 9,9,0,0,0,0,0,0,0 }, cw[0], cw[1], cw[2] };
    for (int i = 3; i < 6; ++i) { iv[i].r = 0; iv[i].g = 1; iv[i].b = 0; iv[i].a = 1; }
    uint16_t idx[3] = { 0, 1, 2 }; ID3D11Buffer *vi = make_buf(iv, sizeof(iv), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT), *ib = make_buf(idx, sizeof(idx), D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DEFAULT);
    cb.tint[0] = cb.tint[1] = cb.tint[2] = 1; ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource *)cbuf, 0, NULL, &cb, 0, 0);
    ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vi, &stride, &off); ID3D11DeviceContext_IASetIndexBuffer(ctx, ib, DXGI_FORMAT_R16_UINT, 0);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_DrawIndexed(ctx, 3, 0, 3);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px,pitch,32,55)[1] == 255, "DrawIndexed(baseVertex=3) reads the second triangle (green)"); free(px);

    printf("[11] scissor\n");
    D3D11_RECT sr = { 0, 0, 32, 64 }; ID3D11DeviceContext_RSSetScissorRects(ctx, 1, &sr); ID3D11DeviceContext_RSSetState(ctx, rs_sc);
    ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vh, &stride, &off); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px,pitch,10,32)[2] > 100 && PX(px,pitch,50,32)[2] == 0, "only x<32 is drawn (left r=%d, right r=%d)", PX(px,pitch,10,32)[2], PX(px,pitch,50,32)[2]); free(px);
    ID3D11DeviceContext_RSSetState(ctx, rs_none);

    printf("[12] pixel shader input not produced by the vertex shader still links\n");
    ID3D11DeviceContext_PSSetShader(ctx, psex, NULL, 0); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px,pitch,32,32)[2] > 100, "draw with a dangling varying produced output"); free(px);
    ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);

    printf("[13] BGRA vertex colour input\n");
    D3D11_INPUT_ELEMENT_DESC el2[3] = { el[0], { "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *lay2; ID3D11Device_CreateInputLayout(dev, el2, 3, ID3D10Blob_GetBufferPointer(vsb), ID3D10Blob_GetBufferSize(vsb), &lay2);
    struct { float p[3]; uint8_t c[4]; float uv[2]; } pv[3] = { { { -1,-1,.5f }, { 0, 0, 255, 255 }, { 0, 0 } }, { { 0,3,.5f }, { 0, 0, 255, 255 }, { 0, 0 } }, { { 3,-1,.5f }, { 0, 0, 255, 255 }, { 0, 0 } } };  /* bytes B,G,R,A = pure red */
    ID3D11Buffer *vpk = make_buf(pv, sizeof(pv), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT); UINT st2 = sizeof(pv[0]);
    ID3D11DeviceContext_IASetInputLayout(ctx, lay2); ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vpk, &st2, &off); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear); ID3D11DeviceContext_Draw(ctx, 3, 0);
    px = readback(bb, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &pitch); CHECK(PX(px,pitch,32,32)[2] == 255 && PX(px,pitch,32,32)[0] == 0, "B8G8R8A8 vertex bytes (B,G,R,A)=(0,0,255,255) render red (r=%d b=%d)", PX(px,pitch,32,32)[2], PX(px,pitch,32,32)[0]); free(px);

    printf("[14] present\n");
    IDXGISwapChain_Present(sc, 1, 0); CHECK(presents == 1, "host present callback invoked");
    CHECK(d3d11gles_take_error_count() == 0, "no GL errors were raised during the whole run");

    printf("\n%d checks, %d failed\n", checks, fails); return fails != 0;
}
