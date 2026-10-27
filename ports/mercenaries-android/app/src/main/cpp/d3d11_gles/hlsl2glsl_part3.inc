/* ===== part 3: expressions ================================================ */
typedef struct { int is_static, is_const, is_uniform, is_extern, rowmajor, nointerp, centroid, dir; } Quals; /* dir: 0 in,1 out,2 inout */

static Ex mk(char *s, Ty t) { Ex e; e.s = s; e.t = t; e.stmt = 0; return e; }
static Ex parse_assign(void);
static Ex parse_expr_comma(void);

static int is_simple(const char *s)
{
    int d = 0;
    for (const char *p = s; *p; ++p) {
        char c = *p;
        if (c == '(' || c == '[') ++d;
        else if (c == ')' || c == ']') { if (--d < 0) return 0; }
        else if (d == 0 && !(isalnum((unsigned char)c) || c == '_' || c == '.')) return 0;
    }
    return d == 0;
}
static char *paren(const char *s) { return is_simple(s) ? (char *)s : S("(%s)", s); }

/* ---- type parsing ---------------------------------------------------------- */
static void parse_quals(Quals *q)
{
    for (;;) {
        Tok *t = cur(); if (t->k != T_ID) break;
        const char *w = t->s;
        if (!strcmp(w, "static")) q->is_static = 1;
        else if (!strcmp(w, "const")) q->is_const = 1;
        else if (!strcmp(w, "uniform")) q->is_uniform = 1;
        else if (!strcmp(w, "extern")) q->is_extern = 1;
        else if (!strcmp(w, "row_major")) q->rowmajor = 1;
        else if (!strcmp(w, "column_major")) q->rowmajor = 0;
        else if (!strcmp(w, "nointerpolation")) q->nointerp = 1;
        else if (!strcmp(w, "centroid")) q->centroid = 1;
        else if (!strcmp(w, "in")) q->dir = 0;
        else if (!strcmp(w, "out")) q->dir = 1;
        else if (!strcmp(w, "inout")) q->dir = 2;
        else if (!strcmp(w, "noperspective") || !strcmp(w, "linear") || !strcmp(w, "sample") || !strcmp(w, "precise") ||
                 !strcmp(w, "volatile") || !strcmp(w, "groupshared") || !strcmp(w, "shared")) { /* ignored */ }
        else break;
        C->pos++;
    }
}
static int texture_dim(const char *n)
{
    if (!strcmp(n, "Texture1D")) return 1; if (!strcmp(n, "Texture2D")) return 2;
    if (!strcmp(n, "Texture3D")) return 3; if (!strcmp(n, "TextureCube")) return 4; return 0;
}
static int at_type_name(const Tok *t)
{
    if (t->k != T_ID) return 0;
    Ty x; const char *n = t->s;
    return !strcmp(n, "void") || numeric_typename(n, &x) || find_struct(n) || find_typedef(n, &x) || texture_dim(n) ||
           !strcmp(n, "SamplerState") || !strcmp(n, "SamplerComparisonState") || !strcmp(n, "vector") || !strcmp(n, "matrix") ||
           !strcmp(n, "struct");
}
static void parse_struct_body(Struct *s);
static Ty parse_type(void)
{
    Tok *t = cur(); if (t->k != T_ID) fail("expected a type but found '%s'", t->s);
    char *n = t->s; Ty ty;
    if (!strcmp(n, "void")) { C->pos++; return ty_void(); }
    if (!strcmp(n, "vector") || !strcmp(n, "matrix")) {
        int isv = n[0] == 'v'; C->pos++; expect_op("<");
        int base; if (!scalar_word(expect_ident(), &base)) fail("bad vector element type");
        expect_op(","); int a = atoi(cur()->s); C->pos++; int b = 1;
        if (!isv) { expect_op(","); b = atoi(cur()->s); C->pos++; }
        expect_op(">"); return isv ? mkty(base, 1, a) : mkty(base, a, b);
    }
    if (numeric_typename(n, &ty)) { C->pos++; return ty; }
    if (find_typedef(n, &ty)) { C->pos++; return ty; }
    Struct *st = find_struct(n);
    if (st) { C->pos++; Ty r = mkty(B_STRUCT, 1, 1); r.st = st; return r; }
    int dim = texture_dim(n);
    if (dim) {
        C->pos++; Ty r = mkty(B_TEX, 1, 1); r.texdim = (uint8_t)dim; r.texbase = B_FLOAT; r.texcols = 4;
        if (accept_op("<")) {
            Ty e; Tok *et = cur();
            if (!numeric_typename(et->s, &e)) fail("unsupported texture element type '%s'", et->s);
            C->pos++; r.texbase = e.base; r.texcols = e.cols; expect_op(">");
        }
        return r;
    }
    if (!strcmp(n, "SamplerState") || !strcmp(n, "SamplerComparisonState")) { C->pos++; return mkty(B_SAMP, 1, 1); }
    fail("unknown type '%s'", n); return ty_void();
}

/* ---- struct field lookup ----------------------------------------------------- */
static Field *find_field(Struct *s, const char *n)
{ for (int i = 0; i < s->nf; ++i) if (!strcmp(s->f[i].name, n)) return &s->f[i]; return NULL; }

/* ---- swizzle ------------------------------------------------------------------- */
static int swz_index(char c, int *set)
{
    switch (c) { case 'x': *set = 0; return 0; case 'y': *set = 0; return 1; case 'z': *set = 0; return 2; case 'w': *set = 0; return 3;
                 case 'r': *set = 1; return 0; case 'g': *set = 1; return 1; case 'b': *set = 1; return 2; case 'a': *set = 1; return 3; }
    return -1;
}
static Ex apply_swizzle(Ex e, const char *swz)
{
    size_t n = strlen(swz); if (n < 1 || n > 4) fail("bad swizzle '.%s'", swz);
    int set0 = -1;
    for (size_t i = 0; i < n; ++i) {
        int set; int idx = swz_index(swz[i], &set);
        if (idx < 0) fail("no member or swizzle '.%s'", swz);
        if (set0 < 0) set0 = set; else if (set0 != set) fail("mixed swizzle sets in '.%s'", swz);
        if (idx >= e.t.cols) fail("swizzle '.%s' exceeds the %d components of the value", swz, e.t.cols);
    }
    Ty rt = with_cols(e.t, (int)n);
    if (e.t.cols == 1) {                                      /* swizzle of a scalar */
        if (n == 1) return mk(e.s, rt);
        return mk(S("%s(%s)", glsl_tyname(rt), e.s), rt);
    }
    return mk(S("%s.%s", paren(e.s), swz), rt);
}

/* ---- literals / constructors ------------------------------------------------------ */
static int total_comps(Ex *a, int n) { int t = 0; for (int i = 0; i < n; ++i) t += is_mat(a[i].t) ? a[i].t.rows * a[i].t.cols : a[i].t.cols; return t; }

static Ex make_ctor(Ty to, Ex *a, int n)
{
    if (!is_num(to)) fail("cannot construct this type");
    if (n == 0) fail("constructor needs arguments");
    for (int i = 0; i < n; ++i) if (!is_num(a[i].t) || a[i].t.arr) fail("bad constructor argument");
    const char *tn = glsl_tyname(to);
    if (is_mat(to)) {
        int need = to.rows * to.cols;
        if (n == 1 && is_scalar(a[0].t)) {                    /* diagonal-less splat: fill every element */
            char *args = S("%s", conv(a[0], ty_f(1)));
            for (int i = 1; i < need; ++i) args = S("%s, %s", args, conv(a[0], ty_f(1)));
            return mk(S("transpose(%s(%s))", glsl_tyname(mkty(B_FLOAT, to.cols, to.rows)), args), to);
        }
        if (n == 1 && is_mat(a[0].t)) return mk(conv(a[0], to), to);
        if (total_comps(a, n) != need) fail("matrix constructor needs %d values", need);
        char *args = "";
        for (int i = 0; i < n; ++i) args = S("%s%s%s", args, i ? ", " : "", a[i].s);
        /* HLSL fills row by row; GLSL fills column by column: build the transpose. */
        return mk(S("transpose(%s(%s))", glsl_tyname(mkty(B_FLOAT, to.cols, to.rows)), args), to);
    }
    int need = to.cols;
    if (n == 1) {
        if (is_scalar(a[0].t) || a[0].t.cols >= need) {
            Ex x = a[0]; if (is_mat(x.t)) fail("cannot construct a vector from a matrix");
            if (x.t.cols > need) { x.s = S("(%s).%s", x.s, swz_of(need)); x.t = with_cols(x.t, need); }
            return mk(S("%s(%s)", tn, x.s), to);
        }
        fail("not enough components for %s", tn);
    }
    if (total_comps(a, n) < need) fail("not enough components for %s", tn);
    char *args = ""; int used = 0;
    for (int i = 0; i < n && used < need; ++i) {
        Ex x = a[i]; int c = x.t.cols;
        if (used + c > need) { x.s = S("(%s).%s", x.s, swz_of(need - used)); c = need - used; }
        args = S("%s%s%s", args, i ? ", " : "", x.s); used += c;
    }
    return mk(S("%s(%s)", tn, args), to);
}

/* ---- intrinsics ----------------------------------------------------------------------- */
static int g_need_lit, g_need_fmod;
static Ty common2(Ty a, Ty b)
{
    if (!is_num(a) || !is_num(b) || a.arr || b.arr) fail("operands must be numeric");
    Ty r;
    if (is_mat(a) || is_mat(b)) {
        if (is_mat(a) && is_mat(b)) { if (a.rows != b.rows || a.cols != b.cols) fail("matrix size mismatch"); return a; }
        return is_mat(a) ? a : b;
    }
    int base = promote_base(a.base, b.base);
    if (a.base == B_BOOL && b.base == B_BOOL) base = B_BOOL;
    int n = (a.cols == 1) ? b.cols : (b.cols == 1) ? a.cols : (a.cols < b.cols ? a.cols : b.cols);
    r = mkty(base, 1, n); return r;
}
static Ty to_floaty(Ty t) { Ty r = t; if (is_num(t) && t.base != B_FLOAT) r.base = B_FLOAT; return r; }

static Ex call_unary_f(const char *gl, Ex a)
{
    if (!is_num(a.t)) fail("intrinsic needs a numeric argument");
    Ty t = to_floaty(a.t); return mk(S("%s(%s)", gl, conv(a, t)), t);
}
static void unify_args(Ex *a, int n, Ty *T, int force_float)
{
    Ty t = a[0].t; if (!is_num(t)) fail("intrinsic needs numeric arguments");
    for (int i = 1; i < n; ++i) t = common2(t, a[i].t);
    if (force_float) t = to_floaty(t);
    *T = t;
}
static char *ua(Ex a, Ty T)
{
    /* convert, but keep a scalar scalar when T is a vector of the same base (GLSL allows genType/scalar for a few funcs) */
    return conv(a, T);
}

static Ex intrinsic(const char *nm, Ex *a, int n, int *handled)
{
    *handled = 1; Ty T;
#define U1(HN, GN) if (!strcmp(nm, HN) && n == 1) return call_unary_f(GN, a[0])
    U1("sin","sin"); U1("cos","cos"); U1("tan","tan"); U1("asin","asin"); U1("acos","acos"); U1("atan","atan");
    U1("sinh","sinh"); U1("cosh","cosh"); U1("tanh","tanh"); U1("exp","exp"); U1("exp2","exp2"); U1("log","log"); U1("log2","log2");
    U1("sqrt","sqrt"); U1("floor","floor"); U1("ceil","ceil"); U1("round","roundEven"); U1("trunc","trunc"); U1("frac","fract");
    U1("rsqrt","inversesqrt"); U1("radians","radians"); U1("degrees","degrees"); U1("ddx","dFdx"); U1("ddy","dFdy"); U1("fwidth","fwidth");
    U1("ddx_fine","dFdx"); U1("ddy_fine","dFdy"); U1("ddx_coarse","dFdx"); U1("ddy_coarse","dFdy");
    U1("normalize","normalize");
#undef U1
    if (!strcmp(nm, "saturate") && n == 1) { Ty t = to_floaty(a[0].t); return mk(S("clamp(%s, 0.0, 1.0)", conv(a[0], t)), t); }
    if (!strcmp(nm, "rcp") && n == 1) { Ty t = to_floaty(a[0].t); return mk(S("(1.0 / %s)", paren(conv(a[0], t))), t); }
    if (!strcmp(nm, "log10") && n == 1) { Ty t = to_floaty(a[0].t); return mk(S("(log2(%s) * 0.30102999566)", conv(a[0], t)), t); }
    if (!strcmp(nm, "abs") && n == 1) return mk(S("abs(%s)", a[0].s), a[0].t);
    if (!strcmp(nm, "sign") && n == 1) {
        Ty f = to_floaty(a[0].t); Ty it = with_base(f, B_INT);
        return mk(S("%s(sign(%s))", glsl_tyname(it), conv(a[0], f)), it);
    }
    if (!strcmp(nm, "length") && n == 1) { Ty t = to_floaty(a[0].t); return mk(S("length(%s)", conv(a[0], t)), ty_f(1)); }
    if (!strcmp(nm, "isnan") || !strcmp(nm, "isinf") || !strcmp(nm, "isfinite")) {
        Ty t = to_floaty(a[0].t); Ty bt = with_base(t, B_BOOL); char *x = conv(a[0], t);
        if (!strcmp(nm, "isnan")) return mk(S("isnan(%s)", x), bt);
        if (!strcmp(nm, "isinf")) return mk(S("isinf(%s)", x), bt);
        if (t.cols == 1) return mk(S("(!isnan(%s) && !isinf(%s))", x, x), bt);
        return mk(S("not(%s(isnan(%s)) || %s(isinf(%s)))", glsl_tyname(bt), x, glsl_tyname(bt), x), bt);  /* never used with vectors in practice */
    }
    if ((!strcmp(nm, "any") || !strcmp(nm, "all")) && n == 1) {
        Ty t = a[0].t; char *b = t.base == B_BOOL ? a[0].s : conv(a[0], with_base(t, B_BOOL));
        if (t.cols == 1) return mk(S("%s", b), ty_b(1));
        return mk(S("%s(%s)", nm, b), ty_b(1));
    }
    if (!strcmp(nm, "asuint") && n == 1) return mk(S("floatBitsToUint(%s)", conv(a[0], to_floaty(a[0].t))), with_base(a[0].t, B_UINT));
    if (!strcmp(nm, "asint") && n == 1) {
        if (a[0].t.base == B_UINT) return mk(S("%s(%s)", glsl_tyname(with_base(a[0].t, B_INT)), a[0].s), with_base(a[0].t, B_INT));
        return mk(S("floatBitsToInt(%s)", conv(a[0], to_floaty(a[0].t))), with_base(a[0].t, B_INT));
    }
    if (!strcmp(nm, "asfloat") && n == 1) {
        if (a[0].t.base == B_FLOAT) return a[0];
        if (a[0].t.base == B_INT) return mk(S("intBitsToFloat(%s)", a[0].s), with_base(a[0].t, B_FLOAT));
        return mk(S("uintBitsToFloat(%s)", a[0].s), with_base(a[0].t, B_FLOAT));
    }
    if (!strcmp(nm, "countbits") && n == 1) return mk(S("bitCount(%s)", a[0].s), a[0].t);
    if (!strcmp(nm, "firstbithigh") && n == 1) return mk(S("findMSB(%s)", a[0].s), with_base(a[0].t, B_INT));
    if (!strcmp(nm, "transpose") && n == 1) { Ty t = a[0].t; Ty r = mkty(t.base, t.cols, t.rows); return mk(S("transpose(%s)", a[0].s), r); }
    if (!strcmp(nm, "mul") && n == 2) {
        Ty x = a[0].t, y = a[1].t;
        if (is_scalar(x) || is_scalar(y)) { Ty t = common2(x, y); t = to_floaty(t); return mk(S("(%s * %s)", conv(a[0], is_scalar(x) ? with_base(x, t.base) : t), conv(a[1], is_scalar(y) ? with_base(y, t.base) : t)), t); }
        if (is_vec(x) && is_vec(y)) { Ty t = common2(x, y); t = to_floaty(t); return mk(S("dot(%s, %s)", conv(a[0], t), conv(a[1], t)), ty_f(1)); }
        if (is_vec(x) && is_mat(y)) { if (x.cols != y.rows) fail("mul: vector length %d does not match matrix rows %d", x.cols, y.rows); return mk(S("(%s * %s)", conv(a[0], to_floaty(x)), a[1].s), ty_f(y.cols)); }
        if (is_mat(x) && is_vec(y)) { if (x.cols != y.cols) fail("mul: matrix columns %d do not match vector length %d", x.cols, y.cols); return mk(S("(%s * %s)", a[0].s, conv(a[1], to_floaty(y))), ty_f(x.rows)); }
        if (is_mat(x) && is_mat(y)) { if (x.cols != y.rows) fail("mul: matrix size mismatch"); return mk(S("(%s * %s)", a[0].s, a[1].s), mkty(B_FLOAT, x.rows, y.cols)); }
        fail("unsupported mul operands");
    }
    if (n == 2 && (!strcmp(nm, "pow") || !strcmp(nm, "atan2") || !strcmp(nm, "step") || !strcmp(nm, "fmod") ||
                   !strcmp(nm, "reflect") || !strcmp(nm, "cross") || !strcmp(nm, "distance") || !strcmp(nm, "dot"))) {
        unify_args(a, 2, &T, 1);
        char *x = conv(a[0], T), *y = conv(a[1], T);
        if (!strcmp(nm, "pow")) return mk(S("pow(%s, %s)", x, y), T);
        if (!strcmp(nm, "atan2")) return mk(S("atan(%s, %s)", x, y), T);
        if (!strcmp(nm, "step")) return mk(S("step(%s, %s)", x, y), T);
        if (!strcmp(nm, "fmod")) { g_need_fmod = 1; return mk(S("(%s - %s * trunc(%s / %s))", paren(x), paren(y), paren(x), paren(y)), T); }
        if (!strcmp(nm, "reflect")) return mk(S("reflect(%s, %s)", x, y), T);
        if (!strcmp(nm, "cross")) return mk(S("cross(%s, %s)", x, y), T);
        if (!strcmp(nm, "distance")) return mk(S("distance(%s, %s)", x, y), ty_f(1));
        return mk(S("dot(%s, %s)", x, y), ty_f(1));
    }
    if (n == 2 && (!strcmp(nm, "min") || !strcmp(nm, "max"))) {
        unify_args(a, 2, &T, 0); return mk(S("%s(%s, %s)", nm, conv(a[0], T), conv(a[1], T)), T);
    }
    if (n == 3 && !strcmp(nm, "clamp")) { unify_args(a, 3, &T, 0); return mk(S("clamp(%s, %s, %s)", conv(a[0], T), conv(a[1], T), conv(a[2], T)), T); }
    if (n == 3 && !strcmp(nm, "lerp")) { unify_args(a, 3, &T, 1); return mk(S("mix(%s, %s, %s)", conv(a[0], T), conv(a[1], T), conv(a[2], T)), T); }
    if (n == 3 && !strcmp(nm, "smoothstep")) { unify_args(a, 3, &T, 1); return mk(S("smoothstep(%s, %s, %s)", conv(a[0], T), conv(a[1], T), conv(a[2], T)), T); }
    if (n == 3 && !strcmp(nm, "mad")) { unify_args(a, 3, &T, 0); return mk(S("(%s * %s + %s)", paren(conv(a[0], T)), paren(conv(a[1], T)), paren(conv(a[2], T))), T); }
    if (n == 3 && !strcmp(nm, "lit")) { g_need_lit = 1; return mk(S("_lit(%s, %s, %s)", conv(a[0], ty_f(1)), conv(a[1], ty_f(1)), conv(a[2], ty_f(1))), ty_f(4)); }
    if (n == 1 && !strcmp(nm, "clip")) {
        Ty t = to_floaty(a[0].t); char *x = conv(a[0], t); Ex r;
        r.s = t.cols == 1 ? S("if (%s < 0.0) discard;", x) : S("if (any(lessThan(%s, %s(0.0)))) discard;", x, glsl_tyname(t));
        r.t = ty_void(); r.stmt = 1; return r;
    }
    if (n == 3 && !strcmp(nm, "sincos")) {
        Ty t = to_floaty(a[0].t); char *x = conv(a[0], t); Ex r;
        r.s = S("{ %s = sin(%s); %s = cos(%s); }", a[1].s, x, a[2].s, x); r.t = ty_void(); r.stmt = 1; return r;
    }
    *handled = 0; return mk("", ty_void());
}

/* ---- texture sampling ----------------------------------------------------------------------- */
static char *sampler_pair(Sym *tex, Sym *samp)
{
    int ss = samp ? samp->slot : HLSL_NO_SLOT;
    char *name = ss == HLSL_NO_SLOT ? S("_t%d", tex->slot) : S("_t%d_s%d", tex->slot, ss);
    HlslResult *r = C->res;
    for (int i = 0; i < r->n_samplers; ++i) if (!strcmp(r->samplers[i].name, name)) return name;
    if (r->n_samplers >= HLSL_MAX_SAMPLERS) fail("too many texture/sampler pairs");
    HlslSamplerUse *u = &r->samplers[r->n_samplers];
    memset(u, 0, sizeof(*u));
    snprintf(u->name, sizeof(u->name), "%s", name);
    u->tex_slot = (uint8_t)tex->slot; u->samp_slot = (uint8_t)ss; u->dim = tex->t.texdim;
    u->base = tex->t.texbase == B_INT ? HLSL_BASE_INT : tex->t.texbase == B_UINT ? HLSL_BASE_UINT : HLSL_BASE_FLOAT;
    u->unit = (uint8_t)r->n_samplers;
    r->n_samplers++;
    const char *pre = tex->t.texbase == B_INT ? "i" : tex->t.texbase == B_UINT ? "u" : "";
    const char *kind = tex->t.texdim == 3 ? "sampler3D" : tex->t.texdim == 4 ? "samplerCube" : "sampler2D";
    EMIT_DECL(S("uniform %s%s %s;\n", pre, kind, name));
    return name;
}

static Ex tex_result(Sym *tex, char *call)
{
    int tb = tex->t.texbase, tc = tex->t.texcols;
    Ty full = mkty(tb, 1, 4), rt = mkty(tb, 1, tc);
    if (tc >= 4) return mk(call, full);
    return mk(S("%s.%s", paren(call), swz_of(tc)), rt);
}

static Ex texture_method(Ex texe, const char *method)
{
    Sym *tex = find_sym(texe.s); if (!tex || tex->kind != SYM_TEX) fail("method call on a non-texture");
    int dim = tex->t.texdim;
    expect_op("("); Ex a[6]; int n = 0;
    if (!is_op(")")) { do { if (n >= 6) fail("too many arguments"); a[n++] = parse_assign(); } while (accept_op(",")); }
    expect_op(")");
    Ty coord = dim == 3 || dim == 4 ? ty_f(3) : ty_f(2);
    if (dim == 1) fail("1D textures are not supported");
    if (!strcmp(method, "Sample") || !strcmp(method, "SampleLevel") || !strcmp(method, "SampleBias") ||
        !strcmp(method, "SampleGrad")) {
        if (n < 2 || a[0].t.base != B_SAMP) fail("%s expects (sampler, coordinate, ...)", method);
        Sym *smp = find_sym(a[0].s);
        char *name = sampler_pair(tex, smp);
        char *uv = conv(a[1], coord); char *call;
        if (!strcmp(method, "Sample")) {
            if (n >= 3) call = S("textureOffset(%s, %s, %s)", name, uv, conv(a[2], mkty(B_INT, 1, dim == 3 ? 3 : 2)));
            else call = S("texture(%s, %s)", name, uv);
        } else if (!strcmp(method, "SampleLevel")) {
            if (n < 3) fail("SampleLevel needs a LOD");
            call = S("textureLod(%s, %s, %s)", name, uv, conv(a[2], ty_f(1)));
        } else if (!strcmp(method, "SampleBias")) {
            if (n < 3) fail("SampleBias needs a bias");
            call = S("texture(%s, %s, %s)", name, uv, conv(a[2], ty_f(1)));
        } else {
            if (n < 4) fail("SampleGrad needs gradients");
            call = S("textureGrad(%s, %s, %s, %s)", name, uv, conv(a[2], coord), conv(a[3], coord));
        }
        return tex_result(tex, call);
    }
    if (!strcmp(method, "Load")) {
        if (n < 1) fail("Load needs a coordinate");
        char *name = sampler_pair(tex, NULL);
        char *call;
        if (dim == 3) { char *c = conv(a[0], mkty(B_INT, 1, 4)); call = S("texelFetch(%s, (%s).xyz, (%s).w)", name, c, c); }
        else if (a[0].t.cols >= 3) { char *c = conv(a[0], mkty(B_INT, 1, 3)); call = S("texelFetch(%s, (%s).xy, (%s).z)", name, c, c); }
        else call = S("texelFetch(%s, %s, 0)", name, conv(a[0], mkty(B_INT, 1, 2)));
        return tex_result(tex, call);
    }
    if (!strcmp(method, "GetDimensions")) {
        char *name = sampler_pair(tex, NULL);
        int first = (n > 2 && dim != 3) || (n > 3 && dim == 3) ? 1 : 0;       /* leading mip-level argument form */
        const char *lod = first ? conv(a[0], ty_i(1)) : "0";
        int nd = dim == 3 ? 3 : 2;
        char *body = S("{ %s _sz = textureSize(%s, %s); ", nd == 3 ? "ivec3" : "ivec2", name, lod);
        static const char *comp[] = { "x", "y", "z" };
        for (int i = 0; i < nd && first + i < n; ++i) {
            Ex v = mk(S("_sz.%s", comp[i]), ty_i(1));
            Sym *o = find_sym(a[first + i].s);
            Ty ot = o ? o->t : ty_u(1);
            body = S("%s%s = %s; ", body, a[first + i].s, conv(v, ot));
        }
        Ex r; r.s = S("%s}", body); r.t = ty_void(); r.stmt = 1; return r;
    }
    fail("unsupported texture method '%s'", method);
    return mk("", ty_void());
}

/* ---- user function calls ------------------------------------------------------------------------ */
static Func *find_func(const char *name, Ex *a, int n)
{
    Func *best = NULL; int best_score = -1;
    for (int i = 0; i < C->nfunc; ++i) {
        Func *f = &C->funcs[i]; if (strcmp(f->name, name) || f->np != n) continue;
        int score = 0, ok = 1;
        for (int k = 0; k < n; ++k) {
            if (ty_eq(f->pt[k], a[k].t)) score += 2;
            else if (is_num(f->pt[k]) && is_num(a[k].t) && !is_mat(f->pt[k]) == !is_mat(a[k].t) && a[k].t.cols >= f->pt[k].cols) score += 1;
            else if (f->pt[k].base == a[k].t.base && f->pt[k].st == a[k].t.st) score += 1;
            else ok = 0;
        }
        if (ok && score > best_score) { best = f; best_score = score; }
    }
    return best;
}

static Ex parse_call_args_and_resolve(const char *name, Ty *ctor_ty)
{
    expect_op("(");
    Ex a[16]; int n = 0;
    if (!is_op(")")) { do { if (n >= 16) fail("too many arguments"); a[n++] = parse_assign(); } while (accept_op(",")); }
    expect_op(")");
    if (ctor_ty) return make_ctor(*ctor_ty, a, n);
    int handled; Ex r = intrinsic(name, a, n, &handled);
    if (handled) return r;
    Func *f = find_func(name, a, n);
    if (!f) fail("unknown function '%s' (or wrong argument types)", name);
    char *args = "";
    for (int i = 0; i < n; ++i) {
        char *s = f->pdir[i] == 0 ? conv(a[i], f->pt[i]) : a[i].s;
        args = S("%s%s%s", args, i ? ", " : "", s);
    }
    return mk(S("%s(%s)", f->g, args), f->ret);
}

/* ---- primary / postfix ----------------------------------------------------------------------------- */
static int looks_like_cast(void)
{
    /* '(' type ')' <start of unary expr>, where the type is numeric */
    if (!is_op("(")) return 0;
    Tok *t1 = peek(1); Ty x;
    if (t1->k != T_ID) return 0;
    if (!(numeric_typename(t1->s, &x) || find_typedef(t1->s, &x) || !strcmp(t1->s, "vector") || !strcmp(t1->s, "matrix"))) return 0;
    if (!strcmp(t1->s, "vector") || !strcmp(t1->s, "matrix")) return 1;
    Tok *t2 = peek(2);
    return t2->k == T_OP && !strcmp(t2->s, ")");
}

static Ex parse_unary(void);

static Ex parse_primary(void)
{
    Tok *t = cur();
    if (t->k == T_INT) { C->pos++; return mk(t->s, ty_i(1)); }
    if (t->k == T_UINT) { C->pos++; return mk(t->s, ty_u(1)); }
    if (t->k == T_FLT) { C->pos++; return mk(t->s, ty_f(1)); }
    if (t->k == T_OP && !strcmp(t->s, "(")) {
        C->pos++; Ex e = parse_expr_comma(); expect_op(")"); e.s = S("(%s)", e.s); return e;
    }
    if (t->k != T_ID) fail("unexpected '%s' in expression", t->s);
    if (!strcmp(t->s, "true"))  { C->pos++; return mk("true", ty_b(1)); }
    if (!strcmp(t->s, "false")) { C->pos++; return mk("false", ty_b(1)); }
    char *name = t->s; Ty ty;
    Tok *nx = peek(1);
    int next_paren = nx->k == T_OP && !strcmp(nx->s, "(");
    if (next_paren && !find_sym(name) && (numeric_typename(name, &ty) || find_typedef(name, &ty))) {
        C->pos++; return parse_call_args_and_resolve(name, &ty);
    }
    if (next_paren && !find_sym(name)) { C->pos++; return parse_call_args_and_resolve(name, NULL); }
    C->pos++;
    Sym *s = find_sym(name);
    if (!s) fail("undeclared identifier '%s'", name);
    if (s->kind == SYM_TEX || s->kind == SYM_SAMP) return mk(s->name, s->t);
    return mk(s->g, s->t);
}

static Ex parse_postfix(void)
{
    Ex e = parse_primary();
    for (;;) {
        if (is_op(".")) {
            C->pos++; char *m = expect_ident();
            if (e.t.base == B_TEX) { e = texture_method(e, m); continue; }
            if (e.t.base == B_STRUCT) {
                Field *f = find_field(e.t.st, m); if (!f) fail("struct '%s' has no member '%s'", e.t.st->name, m);
                e = mk(S("%s.%s", paren(e.s), gid(m)), f->t); continue;
            }
            if (!is_num(e.t) || e.t.arr || is_mat(e.t)) fail("cannot use '.%s' on this value", m);
            e = apply_swizzle(e, m); continue;
        }
        if (is_op("[")) {
            C->pos++; Ex idx = parse_expr_comma(); expect_op("]");
            char *ix = conv(idx, idx.t.base == B_UINT ? ty_u(1) : ty_i(1));
            if (e.t.arr) { Ty el = e.t; el.arr = 0; e = mk(S("%s[%s]", paren(e.s), ix), el); continue; }
            if (is_mat(e.t)) {
                Ty rowt = mkty(B_FLOAT, 1, e.t.cols);
                if (is_op("[")) {                                     /* M[i][j] */
                    C->pos++; Ex j = parse_expr_comma(); expect_op("]");
                    e = mk(S("%s[%s][%s]", paren(e.s), conv(j, ty_i(1)), ix), ty_f(1)); continue;
                }
                char *args = ""; for (int c = 0; c < e.t.cols; ++c) args = S("%s%sMX[%d][%s]", args, c ? ", " : "", c, ix);
                char *m = paren(e.s); char *out = S("%s(%s)", glsl_tyname(rowt), args);
                /* substitute MX with the matrix expression */
                char *buf = (char *)amalloc(strlen(out) + (strlen(m) + 1) * (size_t)e.t.cols + 8); char *w = buf;
                for (const char *p = out; *p;) { if (!strncmp(p, "MX[", 3)) { size_t l = strlen(m); memcpy(w, m, l); w += l; p += 2; } else *w++ = *p++; }
                *w = 0; e = mk(buf, rowt); continue;
            }
            if (is_vec(e.t)) { Ty el = with_cols(e.t, 1); e = mk(S("%s[%s]", paren(e.s), ix), el); }
            else fail("cannot index this value");
            continue;
        }
        if (is_op("++") || is_op("--")) { const char *o = cur()->s; C->pos++; e = mk(S("%s%s", paren(e.s), o), e.t); continue; }
        break;
    }
    return e;
}
