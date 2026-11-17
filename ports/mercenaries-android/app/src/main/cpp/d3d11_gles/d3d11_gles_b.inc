/* ===== part B: views, state objects, shaders, programs, draw, clear, copy == */

/* ---- views --------------------------------------------------------------------------------------- */
static void view_destroy(Obj *o) { View *v = (View *)o; if (v->tex) d3dgl_Release(v->tex); if (v->buf) d3dgl_Release(v->buf); }
HRESULT d3dgl_Device_CreateShaderResourceView(ID3D11Device *d, ID3D11Resource *res, const D3D11_SHADER_RESOURCE_VIEW_DESC *desc, ID3D11ShaderResourceView **out)
{
    (void)d; Obj *o = (Obj *)res; if (!o || !out) return E_INVALIDARG;
    View *v = (View *)new_obj(sizeof(View), K_SRV, view_destroy); if (!v) return E_OUTOFMEMORY;
    if (o->kind == K_TEX) { v->tex = (Tex *)o; REF(o); } else { v->buf = (Buf *)o; REF(o); }
    if (desc) { v->srv = *desc; v->view_fmt = desc->Format; v->stencil_view = desc->Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT; }
    *out = (ID3D11ShaderResourceView *)v; return S_OK;
}
void d3dgl_SRV_GetResource(ID3D11ShaderResourceView *s, ID3D11Resource **r)
{ View *v = (View *)s; Obj *o = v->tex ? &v->tex->o : &v->buf->o; REF(o); *r = (ID3D11Resource *)o; }
HRESULT d3dgl_Device_CreateRenderTargetView(ID3D11Device *d, ID3D11Resource *res, const D3D11_RENDER_TARGET_VIEW_DESC *desc, ID3D11RenderTargetView **out)
{
    (void)d; Obj *o = (Obj *)res; if (!o || o->kind != K_TEX || !out) return E_INVALIDARG;
    View *v = (View *)new_obj(sizeof(View), K_RTV, view_destroy); if (!v) return E_OUTOFMEMORY;
    v->tex = (Tex *)o; REF(o); v->mip = desc ? (int)desc->Texture2D.MipSlice : 0; *out = (ID3D11RenderTargetView *)v; return S_OK;
}
HRESULT d3dgl_Device_CreateDepthStencilView(ID3D11Device *d, ID3D11Resource *res, const D3D11_DEPTH_STENCIL_VIEW_DESC *desc, ID3D11DepthStencilView **out)
{
    (void)d; Obj *o = (Obj *)res; if (!o || o->kind != K_TEX || !out) return E_INVALIDARG;
    View *v = (View *)new_obj(sizeof(View), K_DSV, view_destroy); if (!v) return E_OUTOFMEMORY;
    v->tex = (Tex *)o; REF(o); v->mip = desc ? (int)desc->Texture2D.MipSlice : 0; *out = (ID3D11DepthStencilView *)v; return S_OK;
}

/* ---- state objects ---------------------------------------------------------------------------------- */
static GLenum cmp_func(D3D11_COMPARISON_FUNC f)
{ switch (f) { case D3D11_COMPARISON_NEVER: return GL_NEVER; case D3D11_COMPARISON_LESS: return GL_LESS; case D3D11_COMPARISON_EQUAL: return GL_EQUAL;
  case D3D11_COMPARISON_LESS_EQUAL: return GL_LEQUAL; case D3D11_COMPARISON_GREATER: return GL_GREATER; case D3D11_COMPARISON_NOT_EQUAL: return GL_NOTEQUAL;
  case D3D11_COMPARISON_GREATER_EQUAL: return GL_GEQUAL; default: return GL_ALWAYS; } }
static GLenum stencil_op(D3D11_STENCIL_OP o)
{ switch (o) { case D3D11_STENCIL_OP_ZERO: return GL_ZERO; case D3D11_STENCIL_OP_REPLACE: return GL_REPLACE; case D3D11_STENCIL_OP_INCR_SAT: return GL_INCR;
  case D3D11_STENCIL_OP_DECR_SAT: return GL_DECR; case D3D11_STENCIL_OP_INVERT: return GL_INVERT; case D3D11_STENCIL_OP_INCR: return GL_INCR_WRAP;
  case D3D11_STENCIL_OP_DECR: return GL_DECR_WRAP; default: return GL_KEEP; } }
static GLenum blend_factor(D3D11_BLEND b)
{ switch (b) { case D3D11_BLEND_ZERO: return GL_ZERO; case D3D11_BLEND_ONE: return GL_ONE; case D3D11_BLEND_SRC_COLOR: return GL_SRC_COLOR;
  case D3D11_BLEND_INV_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR; case D3D11_BLEND_SRC_ALPHA: return GL_SRC_ALPHA; case D3D11_BLEND_INV_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
  case D3D11_BLEND_DEST_ALPHA: return GL_DST_ALPHA; case D3D11_BLEND_INV_DEST_ALPHA: return GL_ONE_MINUS_DST_ALPHA; case D3D11_BLEND_DEST_COLOR: return GL_DST_COLOR;
  case D3D11_BLEND_INV_DEST_COLOR: return GL_ONE_MINUS_DST_COLOR; case D3D11_BLEND_SRC_ALPHA_SAT: return GL_SRC_ALPHA_SATURATE;
  case D3D11_BLEND_BLEND_FACTOR: return GL_CONSTANT_COLOR; case D3D11_BLEND_INV_BLEND_FACTOR: return GL_ONE_MINUS_CONSTANT_COLOR; default: return GL_ONE; } }
static GLenum blend_op(D3D11_BLEND_OP o)
{ switch (o) { case D3D11_BLEND_OP_SUBTRACT: return GL_FUNC_SUBTRACT; case D3D11_BLEND_OP_REV_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
  case D3D11_BLEND_OP_MIN: return GL_MIN; case D3D11_BLEND_OP_MAX: return GL_MAX; default: return GL_FUNC_ADD; } }

static HRESULT make_state(int kind, const void *desc, size_t n, void **out)
{
    if (!desc || !out) return E_INVALIDARG;
    StateObj *s = (StateObj *)new_obj(sizeof(StateObj), kind, NULL); if (!s) return E_OUTOFMEMORY;
    memcpy(&s->u, desc, n); *out = s; return S_OK;
}
HRESULT d3dgl_Device_CreateBlendState(ID3D11Device *d, const D3D11_BLEND_DESC *x, ID3D11BlendState **o) { (void)d; return make_state(K_BLEND, x, sizeof(*x), (void **)o); }
HRESULT d3dgl_Device_CreateDepthStencilState(ID3D11Device *d, const D3D11_DEPTH_STENCIL_DESC *x, ID3D11DepthStencilState **o) { (void)d; return make_state(K_DS, x, sizeof(*x), (void **)o); }
HRESULT d3dgl_Device_CreateRasterizerState(ID3D11Device *d, const D3D11_RASTERIZER_DESC *x, ID3D11RasterizerState **o) { (void)d; return make_state(K_RS, x, sizeof(*x), (void **)o); }
void d3dgl_RSState_GetDesc(ID3D11RasterizerState *s, D3D11_RASTERIZER_DESC *d) { *d = ((StateObj *)s)->u.rs; }
void d3dgl_BlendState_GetDesc(ID3D11BlendState *s, D3D11_BLEND_DESC *d) { *d = ((StateObj *)s)->u.blend; }

static GLenum wrap_mode(D3D11_TEXTURE_ADDRESS_MODE m)
{ switch (m) { case D3D11_TEXTURE_ADDRESS_WRAP: return GL_REPEAT; case D3D11_TEXTURE_ADDRESS_MIRROR: return GL_MIRRORED_REPEAT; default: return GL_CLAMP_TO_EDGE; } }
static void sampler_destroy(Obj *o) { Sampler *s = (Sampler *)o; if (s->gl) glDeleteSamplers(1, &s->gl); if (s->gl_point) glDeleteSamplers(1, &s->gl_point); }
HRESULT d3dgl_Device_CreateSamplerState(ID3D11Device *dev, const D3D11_SAMPLER_DESC *d, ID3D11SamplerState **out)
{
    (void)dev; if (!d || !out) return E_INVALIDARG;
    Sampler *s = (Sampler *)new_obj(sizeof(Sampler), K_SAMP, sampler_destroy); if (!s) return E_OUTOFMEMORY;
    s->d = *d; int f = (int)d->Filter; int aniso = f == D3D11_FILTER_ANISOTROPIC;
    int minlin = aniso || (f & 0x10), maglin = aniso || (f & 0x04), miplin = aniso || (f & 0x01);
    GLenum mn = minlin ? (miplin ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST) : (miplin ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
    glGenSamplers(1, &s->gl); glGenSamplers(1, &s->gl_point);
    glSamplerParameteri(s->gl, GL_TEXTURE_MIN_FILTER, (GLint)mn); glSamplerParameteri(s->gl, GL_TEXTURE_MAG_FILTER, maglin ? GL_LINEAR : GL_NEAREST);
    glSamplerParameteri(s->gl, GL_TEXTURE_WRAP_S, (GLint)wrap_mode(d->AddressU)); glSamplerParameteri(s->gl, GL_TEXTURE_WRAP_T, (GLint)wrap_mode(d->AddressV));
    glSamplerParameteri(s->gl, GL_TEXTURE_WRAP_R, (GLint)wrap_mode(d->AddressW));
    float maxl = d->MaxLOD > 1000.f ? 1000.f : d->MaxLOD; glSamplerParameterf(s->gl, GL_TEXTURE_MIN_LOD, d->MinLOD); glSamplerParameterf(s->gl, GL_TEXTURE_MAX_LOD, maxl);
    glSamplerParameteri(s->gl_point, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glSamplerParameteri(s->gl_point, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(s->gl_point, GL_TEXTURE_WRAP_S, (GLint)wrap_mode(d->AddressU)); glSamplerParameteri(s->gl_point, GL_TEXTURE_WRAP_T, (GLint)wrap_mode(d->AddressV));
    glSamplerParameteri(s->gl_point, GL_TEXTURE_WRAP_R, (GLint)wrap_mode(d->AddressW));
    *out = (ID3D11SamplerState *)s; return S_OK;
}

/* ---- shader blobs / shaders / layouts ----------------------------------------------------------------- */
#define BLOB_MAGIC 0x31534C47u   /* "GLS1" */
typedef struct { uint32_t magic, stage, gs, glsl_len; HlslResult r; } BlobHdr;
typedef struct { Obj o; void *data; SIZE_T size; } Blob;
static void blob_destroy(Obj *o) { free(((Blob *)o)->data); }
static Blob *blob_new(const void *data, SIZE_T size)
{
    Blob *b = (Blob *)new_obj(sizeof(Blob), K_BLOB, blob_destroy); if (!b) return NULL;
    b->data = malloc(size ? size : 1); if (!b->data) { free(b); return NULL; }
    if (data) memcpy(b->data, data, size); b->size = size; return b;
}
void *d3dgl_Blob_GetBufferPointer(ID3D10Blob *b) { return ((Blob *)b)->data; }
SIZE_T d3dgl_Blob_GetBufferSize(ID3D10Blob *b) { return ((Blob *)b)->size; }

static void shader_destroy(Obj *o) { free(((Shader *)o)->glsl); }
static HRESULT make_shader(int kind, int stage, const void *code, SIZE_T len, void **out)
{
    if (!out) return E_INVALIDARG;
    const BlobHdr *h = (const BlobHdr *)code;
    if (!code || len < sizeof(BlobHdr) || h->magic != BLOB_MAGIC) return E_INVALIDARG;
    Shader *s = (Shader *)new_obj(sizeof(Shader), kind, shader_destroy); if (!s) return E_OUTOFMEMORY;
    s->stage = stage; s->r = h->r; s->r.glsl = NULL; s->unsupported = (int)h->gs;
    if (!h->gs) {
        if (len < sizeof(BlobHdr) + h->glsl_len) { d3dgl_Release(s); return E_INVALIDARG; }
        s->glsl = (char *)malloc((size_t)h->glsl_len + 1); memcpy(s->glsl, (const char *)code + sizeof(BlobHdr), h->glsl_len); s->glsl[h->glsl_len] = 0;
    }
    *out = s; return S_OK;
}
HRESULT d3dgl_Device_CreateVertexShader(ID3D11Device *d, const void *c, SIZE_T n, ID3D11ClassLinkage *l, ID3D11VertexShader **o) { (void)d; (void)l; return make_shader(K_VS, HLSL_STAGE_VS, c, n, (void **)o); }
HRESULT d3dgl_Device_CreatePixelShader(ID3D11Device *d, const void *c, SIZE_T n, ID3D11ClassLinkage *l, ID3D11PixelShader **o) { (void)d; (void)l; return make_shader(K_PS, HLSL_STAGE_PS, c, n, (void **)o); }
HRESULT d3dgl_Device_CreateGeometryShader(ID3D11Device *d, const void *c, SIZE_T n, ID3D11ClassLinkage *l, ID3D11GeometryShader **o)
{
    (void)d; (void)l; if (!o) return E_INVALIDARG;
    /* Geometry shaders do not exist in GLES 3.0: keep a placeholder so the pipeline setup succeeds;
     * draws fall back to the un-expanded primitives. */
    (void)c; (void)n; Shader *s = (Shader *)new_obj(sizeof(Shader), K_GS, shader_destroy); if (!s) return E_OUTOFMEMORY;
    s->unsupported = 1; *o = (ID3D11GeometryShader *)s; return S_OK;
}

static void layout_destroy(Obj *o) { (void)o; }
HRESULT d3dgl_Device_CreateInputLayout(ID3D11Device *dev, const D3D11_INPUT_ELEMENT_DESC *el, UINT n, const void *code, SIZE_T len, ID3D11InputLayout **out)
{
    (void)dev; const BlobHdr *h = (const BlobHdr *)code;
    if (!el || !out || n > 32 || !code || len < sizeof(BlobHdr) || h->magic != BLOB_MAGIC) return E_INVALIDARG;
    Layout *l = (Layout *)new_obj(sizeof(Layout), K_LAYOUT, layout_destroy); if (!l) return E_OUTOFMEMORY;
    l->n = (int)n;
    for (UINT i = 0; i < n; ++i) {
        l->el[i] = el[i]; snprintf(l->names[i], sizeof(l->names[i]), "%s", el[i].SemanticName); l->el[i].SemanticName = l->names[i];
        char up[32]; size_t k = 0; for (; l->names[i][k] && k < 31; ++k) up[k] = (char)toupper((unsigned char)l->names[i][k]); up[k] = 0;
        l->loc[i] = -1;
        for (int a = 0; a < h->r.n_inputs; ++a) if (!strcmp(h->r.inputs[a].semantic, up) && h->r.inputs[a].index == el[i].SemanticIndex) l->loc[i] = h->r.inputs[a].location;
    }
    *out = (ID3D11InputLayout *)l; return S_OK;
}

/* ---- programs ------------------------------------------------------------------------------------------------ */
static GLuint compile_stage(GLenum type, const char *src, const char *what)
{
    GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, NULL); glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, sizeof(log), NULL, log); D3DGL_LOG("%s shader failed to compile: %s", what, log); glDeleteShader(s); return 0; }
    return s;
}
static char *str_replace_once(const char *s, const char *needle, const char *repl)
{
    const char *p = strstr(s, needle); size_t nl = strlen(needle), rl = strlen(repl), sl = strlen(s);
    char *o = (char *)malloc(sl - (p ? nl : 0) + (p ? rl : 0) + 1); if (!o) return NULL;
    if (!p) { memcpy(o, s, sl + 1); return o; }
    size_t pre = (size_t)(p - s); memcpy(o, s, pre); memcpy(o + pre, repl, rl); memcpy(o + pre + rl, p + nl, sl - pre - nl + 1); return o;
}
static char *build_vs_source(Shader *vs, Shader *ps, uint32_t bgra_mask)
{
    char *decls = (char *)calloc(1, 4096), *assigns = (char *)calloc(1, 4096);
    for (int i = 0; i < ps->r.n_varyings; ++i) {
        const HlslIo *pv = &ps->r.varyings[i]; int found = 0;
        for (int k = 0; k < vs->r.n_varyings; ++k) if (!strcmp(vs->r.varyings[k].semantic, pv->semantic) && vs->r.varyings[k].index == pv->index) found = 1;
        if (found) continue;
        const char *tn = pv->base == HLSL_BASE_INT ? "ivec4" : pv->base == HLSL_BASE_UINT ? "uvec4" : "vec4";
        char line[160];
        snprintf(line, sizeof(line), "%sout %s v_%s%u;\n", (pv->flat || pv->base != HLSL_BASE_FLOAT) ? "flat " : "", tn, pv->semantic, pv->index); strcat(decls, line);
        snprintf(line, sizeof(line), "v_%s%u = %s(0);\n", pv->semantic, pv->index, tn); strcat(assigns, line);
    }
    char *a = str_replace_once(vs->glsl, "//@VS_EXTRA_DECLS", decls);
    char *b = str_replace_once(a, "//@VS_EXTRA_ASSIGN", assigns); free(a); free(decls); free(assigns);
    for (int i = 0; b && i < vs->r.n_inputs; ++i) {
        if (!(bgra_mask & (1u << vs->r.inputs[i].location)) || vs->r.inputs[i].comps != 4) continue;
        char from[80], to[96]; snprintf(from, sizeof(from), " = _in_%s%u;", vs->r.inputs[i].semantic, vs->r.inputs[i].index);
        snprintf(to, sizeof(to), " = _in_%s%u.bgra;", vs->r.inputs[i].semantic, vs->r.inputs[i].index);
        char *n = str_replace_once(b, from, to); free(b); b = n;
    }
    return b;
}
static Program *get_program(Shader *vs, Shader *ps, uint32_t bgra_mask)
{
    for (int i = 0; i < G.nprogs; ++i) if (G.progs[i].vs == vs && G.progs[i].ps == ps && G.progs[i].p->bgra == bgra_mask) return G.progs[i].p;
    if (G.nprogs >= MAX_PROGRAMS) { D3DGL_LOG("program cache full"); return NULL; }
    Program *p = (Program *)calloc(1, sizeof(Program)); if (!p) return NULL;
    char *vsrc = build_vs_source(vs, ps, bgra_mask);
    GLuint v = vsrc ? compile_stage(GL_VERTEX_SHADER, vsrc, "vertex") : 0, f = compile_stage(GL_FRAGMENT_SHADER, ps->glsl, "fragment"); free(vsrc);
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); free(p); return NULL; }
    p->prog = glCreateProgram(); glAttachShader(p->prog, v); glAttachShader(p->prog, f); glLinkProgram(p->prog);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(p->prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(p->prog, sizeof(log), NULL, log); D3DGL_LOG("program link failed: %s", log); glDeleteProgram(p->prog); free(p); return NULL; }
    p->vs = vs; p->ps = ps; p->n_vs = vs->r.n_samplers; p->n_ps = ps->r.n_samplers; p->bgra = bgra_mask; p->ps_unit0 = vs->r.n_samplers;
    if (p->n_vs + p->n_ps > G.max_units) D3DGL_LOG("shader pair needs %d texture units, device has %d", p->n_vs + p->n_ps, G.max_units);
    glUseProgram(p->prog);
    for (int i = 0; i < vs->r.n_cbufs; ++i) { GLuint idx = glGetUniformBlockIndex(p->prog, vs->r.cbufs[i].name); if (idx != GL_INVALID_INDEX) glUniformBlockBinding(p->prog, idx, vs->r.cbufs[i].slot); }
    for (int i = 0; i < ps->r.n_cbufs; ++i) { GLuint idx = glGetUniformBlockIndex(p->prog, ps->r.cbufs[i].name); if (idx != GL_INVALID_INDEX) glUniformBlockBinding(p->prog, idx, 16u + ps->r.cbufs[i].slot); }
    for (int i = 0; i < vs->r.n_samplers; ++i) { GLint l = glGetUniformLocation(p->prog, vs->r.samplers[i].name); if (l >= 0) glUniform1i(l, vs->r.samplers[i].unit); }
    for (int i = 0; i < ps->r.n_samplers; ++i) { GLint l = glGetUniformLocation(p->prog, ps->r.samplers[i].name); if (l >= 0) glUniform1i(l, p->ps_unit0 + ps->r.samplers[i].unit); }
    G.progs[G.nprogs].vs = vs; G.progs[G.nprogs].ps = ps; G.progs[G.nprogs].p = p; G.nprogs++;
    REF(vs); REF(ps);
    return p;
}

/* ---- context state setters / getters -------------------------------------------------------------------------- */
void d3dgl_Ctx_IASetPrimitiveTopology(ID3D11DeviceContext *c, D3D11_PRIMITIVE_TOPOLOGY t) { ((Ctx *)c)->topo = t; }
void d3dgl_Ctx_IAGetPrimitiveTopology(ID3D11DeviceContext *c, D3D11_PRIMITIVE_TOPOLOGY *t) { *t = ((Ctx *)c)->topo; }
void d3dgl_Ctx_IASetInputLayout(ID3D11DeviceContext *c, ID3D11InputLayout *l) { SETREF(((Ctx *)c)->layout, l); }
void d3dgl_Ctx_IAGetInputLayout(ID3D11DeviceContext *c, ID3D11InputLayout **l) { *l = (ID3D11InputLayout *)((Ctx *)c)->layout; REF(*l); }
void d3dgl_Ctx_IASetVertexBuffers(ID3D11DeviceContext *c, UINT start, UINT n, ID3D11Buffer *const *b, const UINT *stride, const UINT *off)
{ Ctx *x = (Ctx *)c; for (UINT i = 0; i < n && start + i < MAX_VB; ++i) { SETREF(x->vb[start + i].b, b ? b[i] : NULL); x->vb[start + i].stride = stride ? stride[i] : 0; x->vb[start + i].offset = off ? off[i] : 0; } }
void d3dgl_Ctx_IASetIndexBuffer(ID3D11DeviceContext *c, ID3D11Buffer *b, DXGI_FORMAT f, UINT off) { Ctx *x = (Ctx *)c; SETREF(x->ib, b); x->ib_fmt = f; x->ib_off = off; }
void d3dgl_Ctx_VSSetShader(ID3D11DeviceContext *c, ID3D11VertexShader *s, ID3D11ClassInstance *const *i, UINT n) { (void)i; (void)n; SETREF(((Ctx *)c)->vs, s); }
void d3dgl_Ctx_PSSetShader(ID3D11DeviceContext *c, ID3D11PixelShader *s, ID3D11ClassInstance *const *i, UINT n) { (void)i; (void)n; SETREF(((Ctx *)c)->ps, s); }
void d3dgl_Ctx_GSSetShader(ID3D11DeviceContext *c, ID3D11GeometryShader *s, ID3D11ClassInstance *const *i, UINT n) { (void)i; (void)n; SETREF(((Ctx *)c)->gs, s); }
void d3dgl_Ctx_VSGetShader(ID3D11DeviceContext *c, ID3D11VertexShader **s, ID3D11ClassInstance **i, UINT *n) { *s = (ID3D11VertexShader *)((Ctx *)c)->vs; REF(*s); if (i) *i = NULL; if (n) *n = 0; }
void d3dgl_Ctx_PSGetShader(ID3D11DeviceContext *c, ID3D11PixelShader **s, ID3D11ClassInstance **i, UINT *n) { *s = (ID3D11PixelShader *)((Ctx *)c)->ps; REF(*s); if (i) *i = NULL; if (n) *n = 0; }
void d3dgl_Ctx_GSGetShader(ID3D11DeviceContext *c, ID3D11GeometryShader **s, ID3D11ClassInstance **i, UINT *n) { *s = (ID3D11GeometryShader *)((Ctx *)c)->gs; REF(*s); if (i) *i = NULL; if (n) *n = 0; }
#define SET_ARR(FN, ARR, T, MAXN) void FN(ID3D11DeviceContext *c, UINT start, UINT n, T *const *v) { Ctx *x = (Ctx *)c; for (UINT i = 0; i < n && start + i < MAXN; ++i) SETREF(x->ARR[start + i], v ? v[i] : NULL); }
#define GET_ARR(FN, ARR, T, MAXN) void FN(ID3D11DeviceContext *c, UINT start, UINT n, T **v) { Ctx *x = (Ctx *)c; for (UINT i = 0; i < n; ++i) { v[i] = (start + i < MAXN) ? (T *)x->ARR[start + i] : NULL; REF(v[i]); } }
SET_ARR(d3dgl_Ctx_VSSetConstantBuffers, cb_vs, ID3D11Buffer, MAX_CB) SET_ARR(d3dgl_Ctx_PSSetConstantBuffers, cb_ps, ID3D11Buffer, MAX_CB) SET_ARR(d3dgl_Ctx_GSSetConstantBuffers, cb_gs, ID3D11Buffer, MAX_CB)
GET_ARR(d3dgl_Ctx_PSGetConstantBuffers, cb_ps, ID3D11Buffer, MAX_CB) GET_ARR(d3dgl_Ctx_GSGetConstantBuffers, cb_gs, ID3D11Buffer, MAX_CB)
SET_ARR(d3dgl_Ctx_PSSetShaderResources, srv_ps, ID3D11ShaderResourceView, MAX_TEXSLOT) SET_ARR(d3dgl_Ctx_VSSetShaderResources, srv_vs, ID3D11ShaderResourceView, MAX_TEXSLOT)
GET_ARR(d3dgl_Ctx_PSGetShaderResources, srv_ps, ID3D11ShaderResourceView, MAX_TEXSLOT)
SET_ARR(d3dgl_Ctx_PSSetSamplers, smp_ps, ID3D11SamplerState, MAX_TEXSLOT) SET_ARR(d3dgl_Ctx_VSSetSamplers, smp_vs, ID3D11SamplerState, MAX_TEXSLOT)
GET_ARR(d3dgl_Ctx_PSGetSamplers, smp_ps, ID3D11SamplerState, MAX_TEXSLOT)
void d3dgl_Ctx_RSSetViewports(ID3D11DeviceContext *c, UINT n, const D3D11_VIEWPORT *v) { Ctx *x = (Ctx *)c; x->nvp = n > MAX_VP ? MAX_VP : n; for (UINT i = 0; i < x->nvp; ++i) x->vp[i] = v[i]; }
void d3dgl_Ctx_RSGetViewports(ID3D11DeviceContext *c, UINT *n, D3D11_VIEWPORT *v) { Ctx *x = (Ctx *)c; if (!v) { *n = x->nvp; return; } UINT k = *n < x->nvp ? *n : x->nvp; for (UINT i = 0; i < k; ++i) v[i] = x->vp[i]; *n = k; }
void d3dgl_Ctx_RSSetState(ID3D11DeviceContext *c, ID3D11RasterizerState *s) { SETREF(((Ctx *)c)->rs, s); }
void d3dgl_Ctx_RSGetState(ID3D11DeviceContext *c, ID3D11RasterizerState **s) { *s = (ID3D11RasterizerState *)((Ctx *)c)->rs; REF(*s); }
void d3dgl_Ctx_RSSetScissorRects(ID3D11DeviceContext *c, UINT n, const D3D11_RECT *r) { Ctx *x = (Ctx *)c; x->nsc = n > MAX_VP ? MAX_VP : n; for (UINT i = 0; i < x->nsc; ++i) x->sc[i] = r[i]; }
void d3dgl_Ctx_OMSetRenderTargets(ID3D11DeviceContext *c, UINT n, ID3D11RenderTargetView *const *r, ID3D11DepthStencilView *d)
{ Ctx *x = (Ctx *)c; for (UINT i = 0; i < MAX_RT; ++i) SETREF(x->rtv[i], (i < n && r) ? r[i] : NULL); SETREF(x->dsv, d); G.fbo_dirty = 1; }
void d3dgl_Ctx_OMGetRenderTargets(ID3D11DeviceContext *c, UINT n, ID3D11RenderTargetView **r, ID3D11DepthStencilView **d)
{ Ctx *x = (Ctx *)c; for (UINT i = 0; i < n && r; ++i) { r[i] = (i < MAX_RT) ? (ID3D11RenderTargetView *)x->rtv[i] : NULL; REF(r[i]); } if (d) { *d = (ID3D11DepthStencilView *)x->dsv; REF(*d); } }
void d3dgl_Ctx_OMSetBlendState(ID3D11DeviceContext *c, ID3D11BlendState *b, const FLOAT *f, UINT m)
{ Ctx *x = (Ctx *)c; SETREF(x->blend, b); for (int i = 0; i < 4; ++i) x->blend_factor[i] = f ? f[i] : 1.f; x->sample_mask = m; }
void d3dgl_Ctx_OMGetBlendState(ID3D11DeviceContext *c, ID3D11BlendState **b, FLOAT *f, UINT *m)
{ Ctx *x = (Ctx *)c; *b = (ID3D11BlendState *)x->blend; REF(*b); if (f) for (int i = 0; i < 4; ++i) f[i] = x->blend_factor[i]; if (m) *m = x->sample_mask; }
void d3dgl_Ctx_OMSetDepthStencilState(ID3D11DeviceContext *c, ID3D11DepthStencilState *s, UINT ref) { Ctx *x = (Ctx *)c; SETREF(x->dss, s); x->stencil_ref = ref; }
void d3dgl_Ctx_OMGetDepthStencilState(ID3D11DeviceContext *c, ID3D11DepthStencilState **s, UINT *ref) { Ctx *x = (Ctx *)c; *s = (ID3D11DepthStencilState *)x->dss; REF(*s); if (ref) *ref = x->stencil_ref; }
void d3dgl_Ctx_Flush(ID3D11DeviceContext *c) { (void)c; glFlush(); }

/* ---- framebuffer binding -------------------------------------------------------------------------------------------- */
static void att_set(GLuint fbo_target, GLenum point, int slot, const View *v)
{
    GLuint tex = 0, target = GL_TEXTURE_2D; GLint lvl = 0;
    if (v && v->tex) { tex = v->tex->tex; target = v->tex->cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : GL_TEXTURE_2D; lvl = v->mip; }
    if (G.att_tex[slot] == tex && G.att_level[slot] == lvl && G.att_target[slot] == target) return;
    glFramebufferTexture2D(fbo_target, point, target, tex, lvl); G.att_tex[slot] = tex; G.att_target[slot] = target; G.att_level[slot] = lvl;
}
static void bind_ctx_fbo(Ctx *c, int *w, int *h)
{
    glBindFramebuffer(GL_FRAMEBUFFER, G.fbo);
    GLenum bufs[MAX_RT]; int last = -1;
    for (int i = 0; i < MAX_RT; ++i) { att_set(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, i, c->rtv[i]); bufs[i] = c->rtv[i] ? GL_COLOR_ATTACHMENT0 + (GLenum)i : GL_NONE; if (c->rtv[i]) last = i; }
    const Fmt *df = c->dsv ? c->dsv->tex->fmt : NULL;
    if (df && df->stencil) { att_set(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, MAX_RT, c->dsv); }
    else {
        /* depth-only or none: make sure a previous depth-stencil attachment is cleared out */
        if (G.att_tex[MAX_RT] && !df) { glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0); G.att_tex[MAX_RT] = 0; }
        else if (df) att_set(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, MAX_RT, c->dsv);
    }
    if (last < 0) { GLenum none = GL_NONE; glDrawBuffers(1, &none); } else glDrawBuffers(last + 1, bufs);
    const View *ref = c->rtv[0] ? c->rtv[0] : c->dsv; UINT mw = 0, mh = 0;
    if (ref) { mw = dimu(ref->tex->d.Width, (UINT)ref->mip); mh = dimu(ref->tex->d.Height, (UINT)ref->mip); }
    *w = (int)mw; *h = (int)mh; G.fbo_dirty = 0;
}
static void invalidate_att(void) { memset(G.att_tex, 0, sizeof(G.att_tex)); memset(G.att_target, 0, sizeof(G.att_target)); memset(G.att_level, 0, sizeof(G.att_level)); G.fbo_dirty = 1; }

/* ---- state application + draw --------------------------------------------------------------------------------------------- */
static void apply_state(Ctx *c, int rt_w, int rt_h)
{
    const D3D11_RASTERIZER_DESC *rs = c->rs ? &c->rs->u.rs : NULL;
    D3D11_VIEWPORT vp = c->nvp ? c->vp[0] : (D3D11_VIEWPORT){ 0, 0, (FLOAT)rt_w, (FLOAT)rt_h, 0.f, 1.f };
    glViewport((GLint)vp.TopLeftX, (GLint)vp.TopLeftY, (GLsizei)vp.Width, (GLsizei)vp.Height); glDepthRangef(vp.MinDepth, vp.MaxDepth);
    if (rs && rs->ScissorEnable && c->nsc) { glEnable(GL_SCISSOR_TEST); glScissor(c->sc[0].left, c->sc[0].top, c->sc[0].right - c->sc[0].left, c->sc[0].bottom - c->sc[0].top); }
    else glDisable(GL_SCISSOR_TEST);
    /* Rendering is vertically flipped (see hlsl2glsl.h), so D3D's clockwise front face is GL's counter-clockwise. */
    glFrontFace(rs && rs->FrontCounterClockwise ? GL_CW : GL_CCW);
    if (!rs || rs->CullMode == D3D11_CULL_NONE) glDisable(GL_CULL_FACE); else { glEnable(GL_CULL_FACE); glCullFace(rs->CullMode == D3D11_CULL_FRONT ? GL_FRONT : GL_BACK); }
    if (rs && (rs->DepthBias || rs->SlopeScaledDepthBias != 0.f)) { glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(rs->SlopeScaledDepthBias, (GLfloat)rs->DepthBias); } else glDisable(GL_POLYGON_OFFSET_FILL);
    const D3D11_DEPTH_STENCIL_DESC *ds = c->dss ? &c->dss->u.ds : NULL;
    if (ds && ds->DepthEnable) { glEnable(GL_DEPTH_TEST); glDepthFunc(cmp_func(ds->DepthFunc)); glDepthMask(ds->DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL ? GL_TRUE : GL_FALSE); }
    else { glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); }
    if (ds && ds->StencilEnable) {
        glEnable(GL_STENCIL_TEST);
        glStencilFuncSeparate(GL_FRONT, cmp_func(ds->FrontFace.StencilFunc), (GLint)c->stencil_ref, ds->StencilReadMask);
        glStencilFuncSeparate(GL_BACK, cmp_func(ds->BackFace.StencilFunc), (GLint)c->stencil_ref, ds->StencilReadMask);
        glStencilOpSeparate(GL_FRONT, stencil_op(ds->FrontFace.StencilFailOp), stencil_op(ds->FrontFace.StencilDepthFailOp), stencil_op(ds->FrontFace.StencilPassOp));
        glStencilOpSeparate(GL_BACK, stencil_op(ds->BackFace.StencilFailOp), stencil_op(ds->BackFace.StencilDepthFailOp), stencil_op(ds->BackFace.StencilPassOp));
        glStencilMaskSeparate(GL_FRONT_AND_BACK, ds->StencilWriteMask);
    } else glDisable(GL_STENCIL_TEST);
    const D3D11_BLEND_DESC *bs = c->blend ? &c->blend->u.blend : NULL; const D3D11_RENDER_TARGET_BLEND_DESC *b0 = bs ? &bs->RenderTarget[0] : NULL;
    if (b0 && b0->BlendEnable) {
        glEnable(GL_BLEND); glBlendFuncSeparate(blend_factor(b0->SrcBlend), blend_factor(b0->DestBlend), blend_factor(b0->SrcBlendAlpha), blend_factor(b0->DestBlendAlpha));
        glBlendEquationSeparate(blend_op(b0->BlendOp), blend_op(b0->BlendOpAlpha)); glBlendColor(c->blend_factor[0], c->blend_factor[1], c->blend_factor[2], c->blend_factor[3]);
    } else glDisable(GL_BLEND);
    unsigned m = b0 ? b0->RenderTargetWriteMask : 15u; glColorMask((m & 1) != 0, (m & 2) != 0, (m & 4) != 0, (m & 8) != 0);
    if (bs && bs->AlphaToCoverageEnable) glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE); else glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

static void bind_resources(Ctx *c, Program *p)
{
    for (int stage = 0; stage < 2; ++stage) {
        const Shader *s = stage ? p->ps : p->vs; int unit0 = stage ? p->ps_unit0 : 0;
        View **srv = stage ? c->srv_ps : c->srv_vs; Sampler **smp = stage ? c->smp_ps : c->smp_vs;
        for (int i = 0; i < s->r.n_samplers; ++i) {
            const HlslSamplerUse *u = &s->r.samplers[i]; GLuint unit = (GLuint)(unit0 + u->unit);
            View *v = u->tex_slot < MAX_TEXSLOT ? srv[u->tex_slot] : NULL; Tex *t = v ? v->tex : NULL;
            GLenum target = u->dim == HLSL_DIM_CUBE ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
            glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(target, (t && !t->staging) ? t->tex : 0);
            Sampler *sm = (u->samp_slot != HLSL_NO_SLOT && u->samp_slot < MAX_TEXSLOT) ? smp[u->samp_slot] : NULL;
            int force_point = !sm || !t || t->fmt->depth || t->fmt->integer || u->samp_slot == HLSL_NO_SLOT;
            glBindSampler(unit, (sm && !force_point) ? sm->gl : (sm ? sm->gl_point : 0));
            if (force_point && !sm) { /* no sampler bound: texture's own (default) parameters apply, force nearest */
                glBindSampler(unit, 0); if (t && !t->staging) { glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); } }
        }
        Buf **cbs = stage ? c->cb_ps : c->cb_vs;
        for (int i = 0; i < s->r.n_cbufs; ++i) { UINT slot = s->r.cbufs[i].slot; glBindBufferBase(GL_UNIFORM_BUFFER, (stage ? 16u : 0u) + slot, (slot < MAX_CB && cbs[slot]) ? cbs[slot]->buf : 0); }
    }
}

typedef struct { GLint size; GLenum type; GLboolean norm; int is_int; } VFmt;
static int vertex_fmt(DXGI_FORMAT f, VFmt *o)
{
    switch (f) {
    case DXGI_FORMAT_R32G32B32A32_FLOAT: *o = (VFmt){ 4, GL_FLOAT, 0, 0 }; return 1; case DXGI_FORMAT_R32G32B32_FLOAT: *o = (VFmt){ 3, GL_FLOAT, 0, 0 }; return 1;
    case DXGI_FORMAT_R32G32_FLOAT: *o = (VFmt){ 2, GL_FLOAT, 0, 0 }; return 1; case DXGI_FORMAT_R32_FLOAT: *o = (VFmt){ 1, GL_FLOAT, 0, 0 }; return 1;
    case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM: *o = (VFmt){ 4, GL_UNSIGNED_BYTE, 1, 0 }; return 1;
    case DXGI_FORMAT_R16G16B16A16_SNORM: *o = (VFmt){ 4, GL_SHORT, 1, 0 }; return 1; case DXGI_FORMAT_R16G16_SNORM: *o = (VFmt){ 2, GL_SHORT, 1, 0 }; return 1;
    case DXGI_FORMAT_R16_SNORM: *o = (VFmt){ 1, GL_SHORT, 1, 0 }; return 1;
    case DXGI_FORMAT_R16G16B16A16_SINT: *o = (VFmt){ 4, GL_SHORT, 0, 1 }; return 1; case DXGI_FORMAT_R16G16_SINT: *o = (VFmt){ 2, GL_SHORT, 0, 1 }; return 1;
    case DXGI_FORMAT_R16_SINT: *o = (VFmt){ 1, GL_SHORT, 0, 1 }; return 1; case DXGI_FORMAT_R16_UINT: *o = (VFmt){ 1, GL_UNSIGNED_SHORT, 0, 1 }; return 1;
    case DXGI_FORMAT_R32_UINT: *o = (VFmt){ 1, GL_UNSIGNED_INT, 0, 1 }; return 1; case DXGI_FORMAT_R32_SINT: *o = (VFmt){ 1, GL_INT, 0, 1 }; return 1;
    case DXGI_FORMAT_R8_UNORM: *o = (VFmt){ 1, GL_UNSIGNED_BYTE, 1, 0 }; return 1; case DXGI_FORMAT_R8G8_UNORM: *o = (VFmt){ 2, GL_UNSIGNED_BYTE, 1, 0 }; return 1;
    default: return 0; }
}
static UINT vertex_fmt_bytes(DXGI_FORMAT f) { VFmt v; if (!vertex_fmt(f, &v)) return 0; return (UINT)v.size * (v.type == GL_FLOAT || v.type == GL_INT || v.type == GL_UNSIGNED_INT ? 4u : v.type == GL_SHORT || v.type == GL_UNSIGNED_SHORT ? 2u : 1u); }

static int prepare_draw(Ctx *c, int base_vertex, Program **out_prog)
{
    if (!c->vs || !c->ps || !c->vs->glsl || !c->ps->glsl || !c->layout) return 0;
    uint32_t bgra = 0; Layout *L = c->layout;
    for (int i = 0; i < L->n; ++i) if (L->el[i].Format == DXGI_FORMAT_B8G8R8A8_UNORM && L->loc[i] >= 0) bgra |= 1u << L->loc[i];
    Program *p = get_program(c->vs, c->ps, bgra); if (!p) return 0;
    int w = 0, h = 0; bind_ctx_fbo(c, &w, &h); apply_state(c, w, h);
    glUseProgram(p->prog); bind_resources(c, p);
    glBindVertexArray(G.vao); UINT running[MAX_VB] = { 0 }; uint32_t used = 0;
    for (int i = 0; i < L->n; ++i) {
        const D3D11_INPUT_ELEMENT_DESC *e = &L->el[i]; if (e->InputSlot >= MAX_VB) continue;
        UINT off = e->AlignedByteOffset == 0xFFFFFFFFu ? running[e->InputSlot] : e->AlignedByteOffset; running[e->InputSlot] = off + vertex_fmt_bytes(e->Format);
        if (L->loc[i] < 0) continue; VBind *vb = &c->vb[e->InputSlot]; VFmt f; if (!vb->b || !vertex_fmt(e->Format, &f)) continue;
        GLuint loc = (GLuint)L->loc[i]; int shader_int = 0;
        for (int a = 0; a < p->vs->r.n_inputs; ++a) if (p->vs->r.inputs[a].location == loc) shader_int = p->vs->r.inputs[a].base != HLSL_BASE_FLOAT;
        glBindBuffer(GL_ARRAY_BUFFER, vb->b->buf);
        const void *ptr = (const void *)(uintptr_t)((size_t)vb->offset + off + (size_t)(base_vertex > 0 ? base_vertex : 0) * vb->stride);
        if (shader_int && f.is_int) glVertexAttribIPointer(loc, f.size, f.type, (GLsizei)vb->stride, ptr);
        else glVertexAttribPointer(loc, f.size, f.type, f.norm, (GLsizei)vb->stride, ptr);
        glVertexAttribDivisor(loc, e->InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA ? (e->InstanceDataStepRate ? e->InstanceDataStepRate : 1u) : 0u);
        glEnableVertexAttribArray(loc); used |= 1u << loc;
    }
    for (GLuint a = 0; a < 16; ++a) if (!(used & (1u << a))) { glDisableVertexAttribArray(a); glVertexAttrib4f(a, 0.f, 0.f, 0.f, 1.f); }
    *out_prog = p; return 1;
}
static GLenum gl_topology(D3D11_PRIMITIVE_TOPOLOGY t)
{ switch (t) { case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST: return GL_POINTS; case D3D11_PRIMITIVE_TOPOLOGY_LINELIST: return GL_LINES; case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP: return GL_LINE_STRIP;
  case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return GL_TRIANGLE_STRIP; default: return GL_TRIANGLES; } }

void d3dgl_Ctx_Draw(ID3D11DeviceContext *cx, UINT count, UINT start)
{
    Ctx *c = (Ctx *)cx; Program *p; if (!prepare_draw(c, 0, &p)) return;
    glDrawArrays(gl_topology(c->topo), (GLint)start, (GLsizei)count); gl_check("Draw");
}
void d3dgl_Ctx_DrawIndexed(ID3D11DeviceContext *cx, UINT count, UINT start, INT base)
{
    Ctx *c = (Ctx *)cx; Program *p; if (!c->ib || !prepare_draw(c, base, &p)) return;
    int wide = c->ib_fmt == DXGI_FORMAT_R32_UINT; glBindVertexArray(G.vao); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c->ib->buf);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(gl_topology(c->topo), (GLsizei)count, wide ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (const void *)(uintptr_t)((size_t)c->ib_off + (size_t)start * (wide ? 4 : 2)));
    gl_check("DrawIndexed");
}

/* ---- clears -------------------------------------------------------------------------------------------------------------------- */
static void clear_prepare(void) { glDisable(GL_SCISSOR_TEST); glColorMask(1, 1, 1, 1); }
void d3dgl_Ctx_ClearRenderTargetView(ID3D11DeviceContext *cx, ID3D11RenderTargetView *rtv, const FLOAT color[4])
{
    (void)cx; View *v = (View *)rtv; if (!v) return; clear_prepare();
    glBindFramebuffer(GL_FRAMEBUFFER, G.fbo); invalidate_att(); att_set(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, v);
    GLenum b = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &b);
    if (v->tex->fmt->integer) { GLuint u[4] = { (GLuint)color[0], (GLuint)color[1], (GLuint)color[2], (GLuint)color[3] }; glClearBufferuiv(GL_COLOR, 0, u); }
    else glClearBufferfv(GL_COLOR, 0, color);
    invalidate_att(); gl_check("ClearRTV");
}
void d3dgl_Ctx_ClearDepthStencilView(ID3D11DeviceContext *cx, ID3D11DepthStencilView *dsv, UINT flags, FLOAT depth, UINT8 stencil)
{
    (void)cx; View *v = (View *)dsv; if (!v) return; clear_prepare(); glDepthMask(GL_TRUE); glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xFF);
    glBindFramebuffer(GL_FRAMEBUFFER, G.fbo); invalidate_att();
    int st = v->tex->fmt->stencil; att_set(GL_FRAMEBUFFER, st ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, MAX_RT, v);
    GLenum none = GL_NONE; glDrawBuffers(1, &none);
    if ((flags & D3D11_CLEAR_DEPTH) && (flags & D3D11_CLEAR_STENCIL) && st) glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, (GLint)stencil);
    else { if (flags & D3D11_CLEAR_DEPTH) glClearBufferfv(GL_DEPTH, 0, &depth); if ((flags & D3D11_CLEAR_STENCIL) && st) { GLint s = (GLint)stencil; glClearBufferiv(GL_STENCIL, 0, &s); } }
    invalidate_att(); gl_check("ClearDSV");
}

/* ---- copies ---------------------------------------------------------------------------------------------------------------------- */
void d3dgl_Ctx_CopySubresourceRegion(ID3D11DeviceContext *cx, ID3D11Resource *dst, UINT dsub, UINT dx, UINT dy, UINT dz, ID3D11Resource *src, UINT ssub, const D3D11_BOX *box)
{
    (void)cx; (void)dz; Obj *d = (Obj *)dst, *s = (Obj *)src; if (!d || !s) return;
    if (d->kind == K_BUFFER && s->kind == K_BUFFER) {
        Buf *db = (Buf *)d, *sb = (Buf *)s; UINT so = box ? box->left : 0, n = box ? box->right - box->left : sb->d.ByteWidth;
        glBindBuffer(GL_COPY_READ_BUFFER, sb->buf); glBindBuffer(GL_COPY_WRITE_BUFFER, db->buf);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, (GLintptr)so, (GLintptr)dx, (GLsizeiptr)n); gl_check("CopyBuffer"); return;
    }
    if (d->kind != K_TEX || s->kind != K_TEX) return;
    Tex *dt = (Tex *)d, *st = (Tex *)s; UINT sw, sh; sub_info(st, ssub, &sw, &sh, NULL, NULL);
    UINT x0 = box ? box->left : 0, y0 = box ? box->top : 0, w = box ? box->right - box->left : sw, h = box ? box->bottom - box->top : sh;
    if (st->staging && dt->staging) {                                                   /* CPU -> CPU */
        UINT sp = d3d_row_bytes(st->fmt, sw), rows = d3d_rows(st->fmt, h);
        UINT dw; sub_info(dt, dsub, &dw, NULL, NULL, NULL); UINT dp = d3d_row_bytes(dt->fmt, dw);
        for (UINT r = 0; r < rows; ++r) memcpy(dt->cpu[dsub] + (size_t)(dy + r) * dp + d3d_row_bytes(dt->fmt, dx), st->cpu[ssub] + (size_t)(y0 + r) * sp + d3d_row_bytes(st->fmt, x0), d3d_row_bytes(st->fmt, w));
        return;
    }
    if (st->staging) {                                                                  /* CPU -> GPU */
        UINT sp = d3d_row_bytes(st->fmt, sw);
        tex_upload(dt, dsub, dx, dy, w, h, st->cpu[ssub] + (size_t)y0 * sp + d3d_row_bytes(st->fmt, x0), sp); return;
    }
    if (dt->staging) {                                                                  /* GPU -> CPU */
        UINT dw; sub_info(dt, dsub, &dw, NULL, NULL, NULL); UINT dp = d3d_row_bytes(dt->fmt, dw);
        tex_read(st, ssub, x0, y0, w, h, dt->cpu[dsub] + (size_t)dy * dp + d3d_row_bytes(dt->fmt, dx), dp); return;
    }
    /* GPU -> GPU: framebuffer blit (handles format-compatible color and depth/stencil) */
    UINT dmip, dlayer, smip, slayer; sub_info(dt, dsub, NULL, NULL, &dmip, &dlayer); sub_info(st, ssub, NULL, NULL, &smip, &slayer);
    glDisable(GL_SCISSOR_TEST); glColorMask(1, 1, 1, 1); glDepthMask(GL_TRUE); glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xFF);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, G.read_fbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, G.draw_fbo);
    GLbitfield mask; GLenum rp, dp2;
    if (st->fmt->depth) { mask = GL_DEPTH_BUFFER_BIT | (st->fmt->stencil ? GL_STENCIL_BUFFER_BIT : 0); rp = dp2 = st->fmt->stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT; }
    else { mask = GL_COLOR_BUFFER_BIT; rp = dp2 = GL_COLOR_ATTACHMENT0; }
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, rp, face_target(st, slayer), st->tex, (GLint)smip);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, dp2, face_target(dt, dlayer), dt->tex, (GLint)dmip);
    if (!st->fmt->depth) { glReadBuffer(GL_COLOR_ATTACHMENT0); GLenum b = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &b); }
    glBlitFramebuffer((GLint)x0, (GLint)y0, (GLint)(x0 + w), (GLint)(y0 + h), (GLint)dx, (GLint)dy, (GLint)(dx + w), (GLint)(dy + h), mask, GL_NEAREST);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, rp, GL_TEXTURE_2D, 0, 0); glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, dp2, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); invalidate_att(); gl_check("CopyTexture");
}
void d3dgl_Ctx_CopyResource(ID3D11DeviceContext *cx, ID3D11Resource *dst, ID3D11Resource *src)
{
    Obj *s = (Obj *)src; if (!s) return;
    if (s->kind == K_BUFFER) { d3dgl_Ctx_CopySubresourceRegion(cx, dst, 0, 0, 0, 0, src, 0, NULL); return; }
    Tex *t = (Tex *)s; for (int i = 0; i < t->nsub; ++i) d3dgl_Ctx_CopySubresourceRegion(cx, dst, (UINT)i, 0, 0, 0, src, (UINT)i, NULL);
}
