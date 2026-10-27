/* ===== part 5: top level, entry wrapper, API ============================== */
static void split_sem(const char *s, char *name, size_t n, uint32_t *idx)
{
    size_t l = strlen(s), e = l;
    while (e > 0 && isdigit((unsigned char)s[e-1])) --e;
    if (e == l) *idx = 0; else *idx = (uint32_t)atoi(s + e);
    size_t k = e < n - 1 ? e : n - 1;
    for (size_t i = 0; i < k; ++i) name[i] = (char)toupper((unsigned char)s[i]);
    name[k] = 0;
}


/* ---- structs -------------------------------------------------------------------------- */
static Struct *parse_struct_decl(int emit)
{
    /* 'struct' already consumed */
    Struct *st = (Struct *)amalloc(sizeof(Struct)); memset(st, 0, sizeof(*st));
    char *name = (cur()->k == T_ID) ? C->toks[C->pos++].s : S("_anon%d", ++C->anon_counter);
    st->name = name; st->gname = (cur()->k == T_OP && !strcmp(name, "")) ? name : gid(name);
    if (!strncmp(name, "_anon", 5)) st->gname = name;
    expect_op("{");
    while (!is_op("}")) {
        Quals q; memset(&q, 0, sizeof(q)); parse_quals(&q);
        Ty base;
        if (is_id("struct")) { C->pos++; Struct *inner = parse_struct_decl(1); base = mkty(B_STRUCT, 1, 1); base.st = inner; }
        else base = parse_type();
        do {
            char *fname = expect_ident(); Ty t = base;
            if (is_op("[")) { C->pos++; t.arr = (uint16_t)parse_const_int(); expect_op("]"); }
            char sem[32]; parse_semantic_opt(sem, sizeof(sem));
            if (st->nf == st->cap) { st->cap = st->cap ? st->cap * 2 : 8; Field *nf = (Field *)amalloc((size_t)st->cap * sizeof(Field)); if (st->nf) memcpy(nf, st->f, (size_t)st->nf * sizeof(Field)); st->f = nf; }
            Field *f = &st->f[st->nf++]; memset(f, 0, sizeof(*f));
            f->name = fname; f->t = t; snprintf(f->sem, sizeof(f->sem), "%s", sem); f->flat = (uint8_t)q.nointerp; f->centroid = (uint8_t)q.centroid;
        } while (accept_op(","));
        expect_op(";");
    }
    expect_op("}");
    if (emit) {
        Sb o = {0}; sb_add(&o, S("struct %s {\n", st->gname));
        for (int i = 0; i < st->nf; ++i) sb_add(&o, S("  %s;\n", glsl_decl(st->f[i].t, gid(st->f[i].name))));
        if (st->nf == 0) sb_add(&o, "  float _empty;\n");              /* GLSL forbids empty structs */
        sb_add(&o, "};\n"); EMIT_DECL(o.p); sb_free(&o);
        if (C->nst == C->capst) { C->capst = C->capst ? C->capst * 2 : 16; C->structs = (Struct **)realloc(C->structs, (size_t)C->capst * sizeof(Struct *)); }
        C->structs[C->nst++] = st;
    }
    return st;
}

/* ---- cbuffer -------------------------------------------------------------------------- */
static int g_next_cb, g_next_tex, g_next_samp;
static void parse_cbuffer(void)
{
    C->pos++;                                           /* cbuffer / tbuffer */
    char *cbname = expect_ident(); int kind = 0, num = -1;
    skip_register_or_pack(&kind, &num);
    int slot = (kind == 'b' && num >= 0) ? num : g_next_cb++;
    if (kind == 'b') g_next_cb = g_next_cb > num + 1 ? g_next_cb : num + 1;
    if (C->res->n_cbufs >= HLSL_MAX_CBUF || slot >= 15) fail("too many constant buffers");
    const char *stage = C->stage == HLSL_STAGE_VS ? "vs" : "ps";
    char *block = S("cb%d_%s", slot, stage), *inst = S("_cb%d_%s", slot, stage);
    expect_op("{");
    Sb o = {0}; sb_add(&o, S("layout(std140) uniform %s {\n", block));
    struct { char *name; Ty t; } fields[256]; int nf = 0;
    while (!is_op("}")) {
        Quals q; memset(&q, 0, sizeof(q)); parse_quals(&q);
        Ty base;
        if (is_id("struct")) { C->pos++; Struct *inner = parse_struct_decl(1); base = mkty(B_STRUCT, 1, 1); base.st = inner; }
        else base = parse_type();
        do {
            char *fname = expect_ident(); Ty t = base;
            if (is_op("[")) { C->pos++; t.arr = (uint16_t)parse_const_int(); expect_op("]"); }
            int k2 = 0, n2 = -1; skip_register_or_pack(&k2, &n2);
            if (nf >= 256) fail("cbuffer too large");
            if (t.base == B_TEX || t.base == B_SAMP) fail("resources cannot live in a cbuffer");
            sb_add(&o, S("  %s%s;\n", (q.rowmajor && is_mat(t)) ? "layout(row_major) " : "", glsl_decl(t, gid(fname))));
            fields[nf].name = fname; fields[nf].t = t; ++nf;
        } while (accept_op(","));
        expect_op(";");
    }
    expect_op("}"); accept_op(";");
    sb_add(&o, S("} %s;\n", inst));
    EMIT_DECL(o.p); sb_free(&o);
    for (int i = 0; i < nf; ++i) { Sym *s = add_sym(fields[i].name, fields[i].t, S("%s.%s", inst, gid(fields[i].name)), SYM_CBUF); s->slot = slot; }
    HlslCBuf *cb = &C->res->cbufs[C->res->n_cbufs++]; snprintf(cb->name, sizeof(cb->name), "%s", block); cb->slot = (uint8_t)slot;
}

/* ---- functions -------------------------------------------------------------------------- */
static Func *g_entry; static const char *g_entry_name;
static void parse_function(Ty ret, char *name)
{
    expect_op("(");
    Func f; memset(&f, 0, sizeof(f)); f.name = name; f.ret = ret;
    f.is_entry = !strcmp(name, g_entry_name);
    f.g = f.is_entry ? "_hlsl_main" : gid(name);
    push_scope();
    Sb ps = {0};
    if (!is_op(")") && !(is_id("void") && peek(1)->k == T_OP && !strcmp(peek(1)->s, ")"))) {
        do {
            if (f.np >= 16) fail("too many parameters");
            Quals q; memset(&q, 0, sizeof(q)); parse_quals(&q);
            Ty t = parse_type(); char *pn = expect_ident();
            if (is_op("[")) { C->pos++; t.arr = (uint16_t)parse_const_int(); expect_op("]"); }
            char sem[32]; parse_semantic_opt(sem, sizeof(sem));
            if (accept_op("=")) parse_assign();                      /* default value: ignored */
            f.pt[f.np] = t; f.pdir[f.np] = (uint8_t)q.dir; f.psem[f.np] = adup(sem);
            char *g = gid(pn);
            const char *dir = q.dir == 1 ? "out " : q.dir == 2 ? "inout " : "";
            if (t.base == B_TEX || t.base == B_SAMP) fail("texture/sampler function parameters are not supported");
            sb_add(&ps, S("%s%s%s", f.np ? ", " : "", dir, glsl_decl(t, g)));
            add_sym(pn, t, g, SYM_LOCAL); f.np++;
        } while (accept_op(","));
    } else if (is_id("void")) C->pos++;
    expect_op(")");
    parse_semantic_opt(f.retsem, sizeof(f.retsem));
    if (C->nfunc == C->capfunc) { C->capfunc = C->capfunc ? C->capfunc * 2 : 16; C->funcs = (Func *)realloc(C->funcs, (size_t)C->capfunc * sizeof(Func)); }
    C->funcs[C->nfunc++] = f; Func *reg = &C->funcs[C->nfunc - 1];
    if (accept_op(";")) { pop_scope(); sb_free(&ps); return; }
    Sb body = {0}; C->cur_ret = ret;
    parse_block_body(&body);
    EMIT_FUNC(S("%s %s(%s)\n", glsl_tyname(ret), f.g, ps.p ? ps.p : ""));
    EMIT_FUNC(body.p); sb_free(&body); sb_free(&ps); pop_scope();
    (void)reg;
}

/* ---- top level --------------------------------------------------------------------------- */
static void parse_toplevel(void)
{
    if (accept_op(";")) return;
    if (is_id("cbuffer") || is_id("tbuffer")) { parse_cbuffer(); return; }
    if (is_id("struct")) { C->pos++; parse_struct_decl(1); expect_op(";"); return; }
    if (is_id("typedef")) {
        C->pos++; Quals q; memset(&q, 0, sizeof(q)); parse_quals(&q);
        Ty t = parse_type(); char *n = expect_ident(); expect_op(";");
        if (C->ntd == C->captd) { C->captd = C->captd ? C->captd * 2 : 8; C->typedefs = realloc(C->typedefs, (size_t)C->captd * sizeof(C->typedefs[0])); }
        C->typedefs[C->ntd].name = n; C->typedefs[C->ntd].t = t; C->ntd++; return;
    }
    if (is_op("[")) {                                              /* [maxvertexcount(n)] etc. */
        if (peek(1)->k == T_ID && !strcmp(peek(1)->s, "maxvertexcount")) C->res->uses_geometry = 1;
        int d = 0; do { if (is_op("[")) d++; else if (is_op("]")) d--; C->pos++; } while (d > 0 && cur()->k != T_EOF); return;
    }
    Quals q; memset(&q, 0, sizeof(q)); parse_quals(&q);
    Ty t = parse_type(); char *name = expect_ident();
    if (is_op("(")) { parse_function(t, name); return; }
    for (;;) {
        Ty vt = t;
        if (is_op("[")) { C->pos++; vt.arr = (uint16_t)parse_const_int(); expect_op("]"); }
        int kind = 0, num = -1; skip_register_or_pack(&kind, &num);
        if (vt.base == B_TEX) {
            Sym *s = add_sym(name, vt, adup(name), SYM_TEX); s->slot = (kind == 't' && num >= 0) ? num : g_next_tex++;
            if (kind == 't' && num >= 0 && g_next_tex <= num) g_next_tex = num + 1;
        } else if (vt.base == B_SAMP) {
            Sym *s = add_sym(name, vt, adup(name), SYM_SAMP); s->slot = (kind == 's' && num >= 0) ? num : g_next_samp++;
            if (kind == 's' && num >= 0 && g_next_samp <= num) g_next_samp = num + 1;
        } else if (q.is_static || q.is_const) {
            char *g = gid(name); char *init = NULL;
            if (accept_op("=")) init = parse_initializer(vt);
            if (q.is_const && !q.is_static && !init) fail("global '%s' is a uniform; declare it inside a cbuffer", name);
            if (init && is_const_expr(init)) { EMIT_DECL(S("const %s = %s;\n", glsl_decl(vt, g), init)); Sym *s = add_sym(name, vt, g, SYM_GLOBAL); s->is_const = 1; }
            else { EMIT_DECL(S("%s;\n", glsl_decl(vt, g))); if (init) EMIT_GINIT(S("%s = %s;\n", g, init)); add_sym(name, vt, g, SYM_GLOBAL); }
        } else fail("global '%s' is a uniform; declare it inside a cbuffer", name);
        if (!accept_op(",")) break;
        name = expect_ident();
    }
    expect_op(";");
}

/* ---- entry wrapper -------------------------------------------------------------------------- */
typedef struct { char *path; Ty t; char sem[32]; uint8_t flat, centroid; } Leaf;
#define MAX_LEAVES 96
static void collect_leaves(const char *expr, Ty t, const char *sem, int flat, int centroid, Leaf *out, int *n)
{
    if (t.base == B_STRUCT) {
        for (int i = 0; i < t.st->nf; ++i) {
            Field *f = &t.st->f[i];
            collect_leaves(S("%s.%s", expr, gid(f->name)), f->t, f->sem[0] ? f->sem : sem, flat | f->flat, centroid | f->centroid, out, n);
        }
        return;
    }
    if (!is_num(t) || t.arr || is_mat(t)) return;
    if (*n >= MAX_LEAVES) fail("entry point has too many inputs/outputs");
    Leaf *l = &out[(*n)++]; l->path = adup(expr); l->t = t; snprintf(l->sem, sizeof(l->sem), "%s", sem); l->flat = (uint8_t)flat; l->centroid = (uint8_t)centroid;
}

static int io_base(Ty t) { return t.base == B_INT ? HLSL_BASE_INT : t.base == B_UINT ? HLSL_BASE_UINT : HLSL_BASE_FLOAT; }
static const char *vec4_name(int base) { return base == HLSL_BASE_INT ? "ivec4" : base == HLSL_BASE_UINT ? "uvec4" : "vec4"; }
static char *pad4(int base, int comps, const char *expr)
{
    if (comps == 4) return (char *)expr;
    const char *z = base == HLSL_BASE_FLOAT ? "0.0" : base == HLSL_BASE_INT ? "0" : "0u";
    const char *one = base == HLSL_BASE_FLOAT ? "1.0" : base == HLSL_BASE_INT ? "1" : "1u";
    const char *vn = vec4_name(base);
    if (comps == 1) return S("%s(%s, %s, %s, %s)", vn, expr, z, z, one);
    if (comps == 2) return S("%s(%s, %s, %s)", vn, expr, z, one);
    return S("%s(%s, %s)", vn, expr, one);
}
static int add_varying(const char *sem, uint32_t idx, int base, int comps, int flat)
{
    HlslResult *r = C->res;
    for (int i = 0; i < r->n_varyings; ++i) if (!strcmp(r->varyings[i].semantic, sem) && r->varyings[i].index == idx) return i;
    if (r->n_varyings >= HLSL_MAX_IO) fail("too many varyings");
    HlslIo *v = &r->varyings[r->n_varyings]; memset(v, 0, sizeof(*v));
    snprintf(v->semantic, sizeof(v->semantic), "%s", sem); v->index = idx; v->base = (uint8_t)base; v->comps = (uint8_t)comps; v->flat = (uint8_t)flat;
    return r->n_varyings++;
}

static void build_wrapper(Func *ef, Sb *iodecl, Sb *mainb)
{
    HlslResult *r = C->res; int vs = C->stage == HLSL_STAGE_VS;
    sb_add(mainb, "void main() {\n");
    if (C->ginit) sb_add(mainb, C->ginit);
    char *args = ""; char *pos_expr = NULL, *psize_expr = NULL; int next_loc = 0;
    Leaf outs[MAX_LEAVES]; int nout = 0;
    for (int i = 0; i < ef->np; ++i) {
        char *pv = S("_p%d", i);
        sb_add(mainb, S("%s;\n", glsl_decl(ef->pt[i], pv)));
        args = S("%s%s%s", args, i ? ", " : "", pv);
        Leaf in[MAX_LEAVES]; int nin = 0;
        if (ef->pdir[i] != 1) collect_leaves(pv, ef->pt[i], ef->psem[i], 0, 0, in, &nin);
        for (int k = 0; k < nin; ++k) {
            Leaf *l = &in[k]; char nm[24]; uint32_t idx; split_sem(l->sem, nm, sizeof(nm), &idx);
            int base = io_base(l->t); int comps = l->t.cols;
            if (!strcmp(nm, "SV_VERTEXID")) { sb_add(mainb, S("%s = %s(gl_VertexID);\n", l->path, glsl_tyname(l->t))); continue; }
            if (!strcmp(nm, "SV_INSTANCEID")) { sb_add(mainb, S("%s = %s(gl_InstanceID);\n", l->path, glsl_tyname(l->t))); continue; }
            if (!strcmp(nm, "SV_ISFRONTFACE")) { sb_add(mainb, S("%s = %s;\n", l->path, conv(mk("gl_FrontFacing", ty_b(1)), l->t))); continue; }
            if (!strcmp(nm, "SV_PRIMITIVEID")) { sb_add(mainb, S("%s = %s(0);\n", l->path, glsl_tyname(l->t))); continue; }
            if (vs) {
                int found = -1;
                for (int a = 0; a < r->n_inputs; ++a) if (!strcmp(r->inputs[a].semantic, nm) && r->inputs[a].index == idx) found = a;
                if (found < 0) {
                    if (r->n_inputs >= HLSL_MAX_IO) fail("too many vertex inputs");
                    HlslIo *v = &r->inputs[r->n_inputs]; memset(v, 0, sizeof(*v));
                    snprintf(v->semantic, sizeof(v->semantic), "%s", nm); v->index = idx; v->base = (uint8_t)base; v->comps = (uint8_t)comps; v->location = (uint8_t)next_loc++;
                    found = r->n_inputs++;
                    sb_add(iodecl, S("layout(location = %d) in %s _in_%s%u;\n", v->location, glsl_tyname(l->t), nm, idx));
                }
                sb_add(mainb, S("%s = _in_%s%u;\n", l->path, nm, idx));
            } else {
                if (!strcmp(nm, "SV_POSITION") || !strcmp(nm, "POSITION")) { sb_add(mainb, S("%s = %s;\n", l->path, conv(mk("gl_FragCoord", ty_f(4)), l->t))); continue; }
                int flat = l->flat || base != HLSL_BASE_FLOAT;
                int vi = add_varying(nm, idx, base, comps, flat); (void)vi;
                char *vn = S("v_%s%u", nm, idx);
                int dup = 0; for (int a = 0; a < vi; ++a) if (!strcmp(r->varyings[a].semantic, nm) && r->varyings[a].index == idx) dup = 1;
                if (!dup && !strstr(iodecl->p ? iodecl->p : "", S(" %s;", vn)))
                    sb_add(iodecl, S("%s%sin %s %s;\n", flat ? "flat " : "", l->centroid ? "centroid " : "", vec4_name(base), vn));
                Ex v = mk(S("%s.%s", vn, swz_of(comps)), mkty(base == HLSL_BASE_FLOAT ? B_FLOAT : base == HLSL_BASE_INT ? B_INT : B_UINT, 1, comps));
                sb_add(mainb, S("%s = %s;\n", l->path, conv(v, l->t)));
            }
        }
        if (ef->pdir[i] != 0) collect_leaves(pv, ef->pt[i], ef->psem[i], 0, 0, outs, &nout);
    }
    if (ef->ret.base == B_VOID) sb_add(mainb, S("_hlsl_main(%s);\n", args));
    else {
        sb_add(mainb, S("%s _ret = _hlsl_main(%s);\n", glsl_tyname(ef->ret), args));
        collect_leaves("_ret", ef->ret, ef->retsem, 0, 0, outs, &nout);
    }
    for (int k = 0; k < nout; ++k) {
        Leaf *l = &outs[k]; char nm[24]; uint32_t idx; split_sem(l->sem, nm, sizeof(nm), &idx);
        int base = io_base(l->t); int comps = l->t.cols;
        if (vs) {
            if (!strcmp(nm, "SV_POSITION") || !strcmp(nm, "POSITION")) { pos_expr = conv(mk(l->path, l->t), ty_f(4)); continue; }
            if (!strcmp(nm, "PSIZE")) { psize_expr = conv(mk(l->path, l->t), ty_f(1)); continue; }
            if (!nm[0]) continue;
            int flat = l->flat || base != HLSL_BASE_FLOAT;
            add_varying(nm, idx, base, comps, flat);
            char *vn = S("v_%s%u", nm, idx);
            if (!strstr(iodecl->p ? iodecl->p : "", S(" %s;", vn)))
                sb_add(iodecl, S("%s%sout %s %s;\n", flat ? "flat " : "", l->centroid ? "centroid " : "", vec4_name(base), vn));
            Ex v = mk(l->path, l->t);
            if (l->t.base == B_BOOL) v = mk(conv(v, ty_f(comps)), ty_f(comps));
            sb_add(mainb, S("%s = %s;\n", vn, pad4(base, comps, v.s)));
        } else {
            if (!strcmp(nm, "SV_DEPTH") || !strcmp(nm, "DEPTH")) { sb_add(mainb, S("gl_FragDepth = %s;\n", conv(mk(l->path, l->t), ty_f(1)))); r->writes_depth = 1; continue; }
            if (!strcmp(nm, "SV_STENCILREF") || !strcmp(nm, "SV_COVERAGE")) continue;       /* not expressible in GLES 3.0 */
            int target = 0;
            if (!strcmp(nm, "SV_TARGET") || !strcmp(nm, "COLOR") || !strcmp(nm, "")) target = (int)idx;
            else continue;
            if (target > 7) fail("render target index out of range");
            Ty ot = l->t; if (ot.base == B_BOOL) ot = with_base(ot, B_FLOAT);
            sb_add(iodecl, S("layout(location = %d) out %s _o_target%d;\n", target, glsl_tyname(ot), target));
            sb_add(mainb, S("_o_target%d = %s;\n", target, conv(mk(l->path, l->t), ot)));
            r->target_base[target] = (uint8_t)io_base(ot); if (target + 1 > r->n_targets) r->n_targets = target + 1;
        }
    }
    if (vs) {
        if (!pos_expr) fail("vertex shader has no SV_POSITION output");
        sb_add(mainb, S("{ vec4 _P = %s; gl_Position = vec4(_P.x, -_P.y, _P.z * 2.0 - _P.w, _P.w); }\n", pos_expr));
        if (psize_expr) sb_add(mainb, S("gl_PointSize = %s;\n", psize_expr));
        sb_add(mainb, "//@VS_EXTRA_ASSIGN\n");
    }
    sb_add(mainb, "}\n");
}

/* ---- API --------------------------------------------------------------------------------------- */
void hlsl_result_free(HlslResult *r) { if (r) { free(r->glsl); r->glsl = NULL; } }

int hlsl_translate(const char *src, size_t len, const char *entry, int stage, HlslResult *out)
{ return hlsl_translate_ex(src, len, entry, stage, NULL, 0, out); }

int hlsl_translate_ex(const char *src, size_t len, const char *entry, int stage,
                      const char *const *defines, int n_defines, HlslResult *out)
{
    Ctx ctx; memset(&ctx, 0, sizeof(ctx)); C = &ctx;
    memset(out, 0, sizeof(*out)); out->stage = stage; ctx.res = out; ctx.stage = stage;
    g_entry_name = entry ? entry : "main"; g_next_cb = g_next_tex = g_next_samp = 0; g_need_lit = g_need_fmod = 0;
    int rc = 0; Sb iodecl = {0}, mainb = {0};
    if (setjmp(ctx.jb)) { snprintf(out->error, sizeof(out->error), "%s", ctx.err); rc = -1; goto done; }
    for (int i = 0; i < n_defines; ++i) {
        Macro m; memset(&m, 0, sizeof(m)); m.name = adup(defines[2*i]); m.nparams = -1;
        TokVec lt = {0}; lex_line(defines[2*i+1] ? defines[2*i+1] : "1", 0, &lt);
        m.nbody = lt.n; m.body = (Tok *)amalloc((size_t)(lt.n + 1) * sizeof(Tok));
        for (int k = 0; k < lt.n; ++k) m.body[k] = lt.v[k];
        free(lt.v); add_macro(m);
    }
    preprocess(src, len);
    push_scope();
    while (cur()->k != T_EOF) parse_toplevel();
    Func *ef = NULL;
    for (int i = 0; i < ctx.nfunc; ++i) if (ctx.funcs[i].is_entry) ef = &ctx.funcs[i];
    if (!ef) fail("entry point '%s' not found", g_entry_name);
    build_wrapper(ef, &iodecl, &mainb);
    {
        Sb f = {0};
        sb_add(&f, "#version 300 es\nprecision highp float;\nprecision highp int;\n"
                   "precision highp sampler2D;\nprecision highp sampler3D;\nprecision highp samplerCube;\n"
                   "precision highp isampler2D;\nprecision highp usampler2D;\nprecision highp isampler3D;\nprecision highp usampler3D;\n");
        if (g_need_lit) sb_add(&f, "vec4 _lit(float ndl, float ndh, float m){ return vec4(1.0, max(ndl,0.0), (ndl > 0.0) ? pow(max(ndh,0.0), m) : 0.0, 1.0); }\n");
        if (ctx.decl) sb_add(&f, ctx.decl);
        if (ctx.funcs_out) sb_add(&f, ctx.funcs_out);
        if (iodecl.p) sb_add(&f, iodecl.p);
        if (stage == HLSL_STAGE_VS) sb_add(&f, "//@VS_EXTRA_DECLS\n");
        sb_add(&f, mainb.p);
        out->glsl = f.p;
    }
done:
    sb_free(&iodecl); sb_free(&mainb);
    for (ArenaBlk *b = ctx.arena; b;) { ArenaBlk *n = b->next; free(b); b = n; }
    free(ctx.decl); free(ctx.funcs_out); free(ctx.ginit); free(ctx.toks); free(ctx.macros); free(ctx.syms); free(ctx.funcs); free(ctx.structs); free(ctx.typedefs);
    C = NULL;
    return rc;
}
