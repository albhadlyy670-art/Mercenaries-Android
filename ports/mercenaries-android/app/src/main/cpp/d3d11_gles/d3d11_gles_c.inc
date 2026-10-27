/* ===== part C: DXGI, swap chain, device creation, D3DCompile ============= */
typedef struct { Obj o; DXGI_SWAP_CHAIN_DESC d; Tex *back; } SwapChain;
typedef struct { Obj o; } Simple;

static void sc_destroy(Obj *o) { SwapChain *s = (SwapChain *)o; if (s->back) d3dgl_Release(s->back); }
static HRESULT sc_make_back(SwapChain *s)
{
    D3D11_TEXTURE2D_DESC t; memset(&t, 0, sizeof(t));
    t.Width = s->d.BufferDesc.Width ? s->d.BufferDesc.Width : 1280; t.Height = s->d.BufferDesc.Height ? s->d.BufferDesc.Height : 720; t.MipLevels = 1; t.ArraySize = 1;
    t.Format = find_fmt_raw(s->d.BufferDesc.Format) ? s->d.BufferDesc.Format : DXGI_FORMAT_R8G8B8A8_UNORM;
    t.SampleDesc.Count = 1; t.Usage = D3D11_USAGE_DEFAULT; t.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    s->d.BufferDesc.Width = t.Width; s->d.BufferDesc.Height = t.Height;
    return d3dgl_Device_CreateTexture2D(NULL, &t, NULL, (ID3D11Texture2D **)&s->back);
}
HRESULT d3dgl_SC_Present(IDXGISwapChain *p, UINT sync, UINT flags)
{
    SwapChain *s = (SwapChain *)p; if (flags & DXGI_PRESENT_TEST) return S_OK;
    glFlush();
    if (G.host.present && s->back) G.host.present(G.host.user, s->back->tex, (int)s->back->d.Width, (int)s->back->d.Height, (int)sync);
    invalidate_att(); gl_check("Present"); return S_OK;
}
HRESULT d3dgl_SC_GetBuffer(IDXGISwapChain *p, UINT idx, const GUID *riid, void **out)
{ (void)riid; SwapChain *s = (SwapChain *)p; if (idx != 0 || !s->back || !out) return E_INVALIDARG; REF(s->back); *out = s->back; return S_OK; }
HRESULT d3dgl_SC_ResizeBuffers(IDXGISwapChain *p, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags)
{
    (void)count; (void)flags; SwapChain *s = (SwapChain *)p;
    if (w) s->d.BufferDesc.Width = w; if (h) s->d.BufferDesc.Height = h; if (fmt != DXGI_FORMAT_UNKNOWN) s->d.BufferDesc.Format = fmt;
    if (s->back) { d3dgl_Release(s->back); s->back = NULL; }
    return sc_make_back(s);
}
HRESULT d3dgl_SC_SetFullscreenState(IDXGISwapChain *p, BOOL fs, IDXGIOutput *o) { (void)p; (void)fs; (void)o; return S_OK; }
HRESULT d3dgl_SC_GetFullscreenState(IDXGISwapChain *p, BOOL *fs, IDXGIOutput **o) { (void)p; if (fs) *fs = TRUE; if (o) *o = NULL; return S_OK; }

static void adapter_fill(WCHAR *desc, size_t n)
{
    const char *r = (const char *)glGetString(GL_RENDERER); char buf[128]; snprintf(buf, sizeof(buf), "%s (OpenGL ES via d3d11_gles)", r ? r : "GPU");
    size_t i = 0; for (; buf[i] && i + 1 < n; ++i) desc[i] = (WCHAR)(unsigned char)buf[i]; desc[i] = 0;
}
HRESULT d3dgl_Adapter_GetDesc(IDXGIAdapter *a, DXGI_ADAPTER_DESC *d) { (void)a; memset(d, 0, sizeof(*d)); adapter_fill(d->Description, 128); d->DedicatedVideoMemory = (SIZE_T)1024 * 1024 * 1024; return S_OK; }
HRESULT d3dgl_Adapter_GetDesc1(IDXGIAdapter *a, DXGI_ADAPTER_DESC1 *d) { (void)a; memset(d, 0, sizeof(*d)); adapter_fill(d->Description, 128); d->DedicatedVideoMemory = (SIZE_T)1024 * 1024 * 1024; return S_OK; }
HRESULT d3dgl_Factory_MakeWindowAssociation(IDXGIFactory *f, HWND w, UINT fl) { (void)f; (void)w; (void)fl; return S_OK; }
HRESULT d3dgl_Factory_CheckFeatureSupport(IDXGIFactory *f, DXGI_FEATURE feat, void *data, UINT size)
{ (void)f; (void)feat; if (data && size >= sizeof(BOOL)) *(BOOL *)data = FALSE; return S_OK; }
HRESULT d3dgl_Factory_EnumAdapterByGpuPreference(IDXGIFactory *f, UINT idx, DXGI_GPU_PREFERENCE pref, const GUID *riid, void **out)
{
    (void)f; (void)pref; (void)riid; if (idx != 0) return DXGI_ERROR_NOT_FOUND;
    Simple *a = (Simple *)new_obj(sizeof(Simple), K_ADAPTER, NULL); if (!a) return E_OUTOFMEMORY; *out = a; return S_OK;
}
HRESULT d3dgl_DXGIDevice_GetAdapter(IDXGIDevice *d, IDXGIAdapter **a) { (void)d; Simple *s = (Simple *)new_obj(sizeof(Simple), K_ADAPTER, NULL); if (!s) return E_OUTOFMEMORY; *a = (IDXGIAdapter *)s; return S_OK; }
HRESULT d3dgl_SC_GetParent(IDXGISwapChain *p, const GUID *riid, void **out)
{ (void)p; (void)riid; Simple *f = (Simple *)new_obj(sizeof(Simple), K_FACTORY, NULL); if (!f) return E_OUTOFMEMORY; *out = f; return S_OK; }
HRESULT CreateDXGIFactory1(const GUID *riid, void **out)
{ (void)riid; Simple *f = (Simple *)new_obj(sizeof(Simple), K_FACTORY, NULL); if (!f) return E_OUTOFMEMORY; *out = f; return S_OK; }

HRESULT d3dgl_QueryInterface(void *p, const GUID *riid, void **out)
{
    (void)riid; Obj *o = (Obj *)p; if (!o || !out) return E_INVALIDARG;
    if (o->kind == K_DEVICE) { Simple *d = (Simple *)new_obj(sizeof(Simple), K_DXGIDEV, NULL); if (!d) return E_OUTOFMEMORY; *out = d; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
HRESULT d3dgl_Device_CheckFeatureSupport(ID3D11Device *d, D3D11_FEATURE f, void *data, UINT size)
{
    (void)d; if (f == D3D11_FEATURE_D3D11_OPTIONS2 && data && size >= sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS2)) { memset(data, 0, size); return S_OK; }
    return E_INVALIDARG;
}
HRESULT d3dgl_Device_GetDeviceRemovedReason(ID3D11Device *d) { (void)d; return S_OK; }

static void ctx_destroy(Obj *o)
{
    Ctx *c = (Ctx *)o; void **slots[] = { (void **)&c->vs, (void **)&c->ps, (void **)&c->gs, (void **)&c->layout, (void **)&c->ib, (void **)&c->dsv, (void **)&c->blend, (void **)&c->dss, (void **)&c->rs };
    for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) if (*slots[i]) d3dgl_Release(*slots[i]);
    for (int i = 0; i < MAX_VB; ++i) if (c->vb[i].b) d3dgl_Release(c->vb[i].b);
    for (int i = 0; i < MAX_CB; ++i) { if (c->cb_vs[i]) d3dgl_Release(c->cb_vs[i]); if (c->cb_ps[i]) d3dgl_Release(c->cb_ps[i]); if (c->cb_gs[i]) d3dgl_Release(c->cb_gs[i]); }
    for (int i = 0; i < MAX_TEXSLOT; ++i) { if (c->srv_ps[i]) d3dgl_Release(c->srv_ps[i]); if (c->srv_vs[i]) d3dgl_Release(c->srv_vs[i]); if (c->smp_ps[i]) d3dgl_Release(c->smp_ps[i]); if (c->smp_vs[i]) d3dgl_Release(c->smp_vs[i]); }
    for (int i = 0; i < MAX_RT; ++i) if (c->rtv[i]) d3dgl_Release(c->rtv[i]);
}
static void gl_init_once(void)
{
    if (G.inited) return;
    glGenVertexArrays(1, &G.vao); glGenFramebuffers(1, &G.fbo); glGenFramebuffers(1, &G.read_fbo); glGenFramebuffers(1, &G.draw_fbo);
    GLint u = 16; glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &u); G.max_units = u;
    const char *dbg = getenv("D3DGL_DEBUG"); G.dbg = dbg && *dbg == '1';
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); G.inited = 1; invalidate_att();
    D3DGL_LOG("GL: %s | %s | %d texture units", (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION), u);
}

HRESULT D3D11CreateDeviceAndSwapChain(IDXGIAdapter *adapter, D3D_DRIVER_TYPE type, HMODULE sw, UINT flags, const D3D_FEATURE_LEVEL *lv, UINT nlv, UINT sdk,
        const DXGI_SWAP_CHAIN_DESC *scd, IDXGISwapChain **out_sc, ID3D11Device **out_dev, D3D_FEATURE_LEVEL *out_lv, ID3D11DeviceContext **out_ctx)
{
    (void)adapter; (void)type; (void)sw; (void)flags; (void)lv; (void)nlv; (void)sdk;
    const GLubyte *ver = glGetString(GL_VERSION);
    if (!ver) { D3DGL_LOG("no current OpenGL ES context: create one before the device"); return E_FAIL; }
    gl_init_once();
    Simple *dev = (Simple *)new_obj(sizeof(Simple), K_DEVICE, NULL); Ctx *ctx = (Ctx *)new_obj(sizeof(Ctx), K_CTX, ctx_destroy);
    if (!dev || !ctx) { free(dev); free(ctx); return E_OUTOFMEMORY; }
    ctx->topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; ctx->sample_mask = 0xFFFFFFFFu; for (int i = 0; i < 4; ++i) ctx->blend_factor[i] = 1.f; G.ctx = ctx;
    if (out_sc && scd) {
        SwapChain *s = (SwapChain *)new_obj(sizeof(SwapChain), K_SC, sc_destroy); if (!s) return E_OUTOFMEMORY;
        s->d = *scd; HRESULT hr = sc_make_back(s); if (FAILED(hr)) { d3dgl_Release(s); return hr; } *out_sc = (IDXGISwapChain *)s;
    }
    if (out_dev) *out_dev = (ID3D11Device *)dev; else d3dgl_Release(dev);
    if (out_ctx) *out_ctx = (ID3D11DeviceContext *)ctx; else d3dgl_Release(ctx);
    if (out_lv) *out_lv = D3D_FEATURE_LEVEL_11_0;
    return S_OK;
}

/* ---- D3DCompile ------------------------------------------------------------------------------------------------------ */
HRESULT D3DCompile(const void *src, SIZE_T len, LPCSTR name, const D3D_SHADER_MACRO *defines, ID3DInclude include, LPCSTR entry, LPCSTR target, UINT f1, UINT f2, ID3D10Blob **code, ID3D10Blob **errors)
{
    (void)name; (void)include; (void)f1; (void)f2; if (code) *code = NULL; if (errors) *errors = NULL;
    if (!src || !target || !code) return E_INVALIDARG;
    int stage = target[0] == 'p' ? HLSL_STAGE_PS : HLSL_STAGE_VS, gs = target[0] == 'g';
    BlobHdr hdr; memset(&hdr, 0, sizeof(hdr)); hdr.magic = BLOB_MAGIC; hdr.stage = (uint32_t)stage;
    if (gs) { hdr.gs = 1; *code = (ID3D10Blob *)blob_new(&hdr, sizeof(hdr)); return *code ? S_OK : E_OUTOFMEMORY; }
    const char *defs[64]; int nd = 0;
    for (; defines && defines[nd].Name && nd < 32; ++nd) { defs[nd * 2] = defines[nd].Name; defs[nd * 2 + 1] = defines[nd].Definition ? defines[nd].Definition : "1"; }
    HlslResult r; int rc = hlsl_translate_ex((const char *)src, (size_t)len, entry ? entry : "main", stage, defs, nd, &r);
    if (rc) {
        if (errors) { char msg[640]; snprintf(msg, sizeof(msg), "%s: %s", name ? name : "shader", r.error); *errors = (ID3D10Blob *)blob_new(msg, strlen(msg) + 1); }
        D3DGL_LOG("HLSL translation failed (%s, %s): %s", name ? name : "?", target, r.error); hlsl_result_free(&r); return E_FAIL;
    }
    size_t gl = strlen(r.glsl); hdr.r = r; hdr.r.glsl = NULL; hdr.glsl_len = (uint32_t)gl;
    Blob *b = blob_new(NULL, sizeof(hdr) + gl); if (!b) { hlsl_result_free(&r); return E_OUTOFMEMORY; }
    memcpy(b->data, &hdr, sizeof(hdr)); memcpy((char *)b->data + sizeof(hdr), r.glsl, gl);
    hlsl_result_free(&r); *code = (ID3D10Blob *)b;
    if (getenv("D3DGL_DUMP_SHADERS")) fprintf(stderr, "----- %s (%s) -----\n%s\n", name ? name : "shader", target, (const char *)b->data + sizeof(hdr));
    return S_OK;
}

HRESULT D3DCreateBlob(SIZE_T size, ID3D10Blob **out)
{
    if (!out) return E_INVALIDARG; Blob *b = blob_new(NULL, size); if (!b) return E_OUTOFMEMORY;
    memset(b->data, 0, size); *out = (ID3D10Blob *)b; return S_OK;
}
