/* ===== part 2: types, symbols, GLSL naming, conversions =================== */
enum { B_VOID, B_BOOL, B_INT, B_UINT, B_FLOAT, B_STRUCT, B_TEX, B_SAMP };
enum { SYM_LOCAL, SYM_GLOBAL, SYM_CBUF, SYM_TEX, SYM_SAMP };

typedef struct { char *s; Ty t; int stmt; } Ex;

static Ty mkty(int base, int rows, int cols) { Ty t; memset(&t, 0, sizeof(t)); t.base = (uint8_t)base; t.rows = (uint8_t)rows; t.cols = (uint8_t)cols; return t; }
static Ty ty_void(void)  { return mkty(B_VOID, 1, 1); }
static Ty ty_f(int n)    { return mkty(B_FLOAT, 1, n); }
static Ty ty_i(int n)    { return mkty(B_INT, 1, n); }
static Ty ty_u(int n)    { return mkty(B_UINT, 1, n); }
static Ty ty_b(int n)    { return mkty(B_BOOL, 1, n); }
static int is_num(Ty t)  { return t.base >= B_BOOL && t.base <= B_FLOAT; }
static int is_scalar(Ty t) { return is_num(t) && !t.arr && t.rows == 1 && t.cols == 1; }
static int is_vec(Ty t)  { return is_num(t) && !t.arr && t.rows == 1 && t.cols > 1; }
static int is_mat(Ty t)  { return is_num(t) && !t.arr && t.rows > 1; }
static Ty with_base(Ty t, int base) { t.base = (uint8_t)base; return t; }
static Ty with_cols(Ty t, int n)    { t.cols = (uint8_t)n; t.rows = 1; return t; }
static int ty_eq(Ty a, Ty b)
{
    return a.base == b.base && a.rows == b.rows && a.cols == b.cols && a.arr == b.arr && a.st == b.st &&
           (a.base != B_TEX || (a.texdim == b.texdim && a.texbase == b.texbase));
}

static const char *k_reserved[] = {
    "input","output","sample","filter","common","partition","active","asm","class","union","enum","typedef","template","this",
    "resource","goto","inline","noinline","public","static","extern","external","interface","long","short","half","fixed","unsigned",
    "superp","namespace","using","cast","packed","precise","smooth","flat","patch","subroutine","shared","buffer","texture","main",
    "mix","step","fract","mod","sign","floor","ceil","abs","min","max","clamp","dot","cross","length","normalize","pow","exp","log","sqrt",
    "sin","cos","tan","atan","asin","acos","round","trunc","reflect","refract","distance","transpose","inverse","lessThan","greaterThan","equal",
    "notEqual","any","all","not","float","int","uint","bool","vec2","vec3","vec4","mat2","mat3","mat4","ivec2","ivec3","ivec4","uvec2","uvec3","uvec4",
    "bvec2","bvec3","bvec4","layout","uniform","varying","attribute","in","out","inout","const","centroid","invariant","lowp","mediump","highp",
    "precision","discard","struct","sampler2D","sampler3D","samplerCube","image2D","atomic_uint","coherent","volatile","restrict","readonly","writeonly",
    "do","else","for","if","return","switch","case","default","while","break","continue","true","false","void","double","dvec2","dvec3","dvec4",
    "determinant","outerProduct","smoothstep","radians","degrees","exp2","log2","inversesqrt","faceforward","modf","isnan","isinf","dFdx","dFdy","fwidth",
    "texelFetch","textureSize","textureLod","textureGrad","textureOffset","matrixCompMult","roundEven","floatBitsToInt","floatBitsToUint","uintBitsToFloat","intBitsToFloat",
    "sinh","cosh","tanh","asinh","acosh","atanh","bitCount","findLSB","findMSB","packHalf2x16","unpackHalf2x16","noise","fma","frexp","ldexp", NULL };

static char *gid(const char *n)
{
    int bad = (!strncmp(n, "gl_", 3)) || strstr(n, "__") != NULL;
    for (int i = 0; !bad && k_reserved[i]; ++i) if (!strcmp(n, k_reserved[i])) bad = 1;
    return bad ? S("%s_u", n) : adup(n);
}

static char *glsl_tyname(Ty t)
{
    static const char *vp[] = { "", "b", "i", "u", "" };
    if (t.base == B_STRUCT) return t.st->gname;
    if (t.base == B_VOID) return "void";
    int b = t.base - B_BOOL;                       /* 0 bool,1 int,2 uint,3 float */
    const char *pre = b == 0 ? "b" : b == 1 ? "i" : b == 2 ? "u" : "";
    (void)vp;
    if (t.rows == 1 && t.cols == 1) return b == 0 ? "bool" : b == 1 ? "int" : b == 2 ? "uint" : "float";
    if (t.rows == 1) return S("%svec%d", pre, t.cols);
    if (t.rows == t.cols) return S("mat%d", t.cols);
    return S("mat%dx%d", t.cols, t.rows);
}
/* "float x[4]" style declarator: returns "type name[N]" */
static char *glsl_decl(Ty t, const char *name)
{
    if (t.arr) { Ty e = t; e.arr = 0; return S("%s %s[%d]", glsl_tyname(e), name, t.arr); }
    return S("%s %s", glsl_tyname(t), name);
}

/* ---- scopes & symbols ---------------------------------------------------------- */
static void push_scope(void) { if (C->nscope >= 63) fail("scopes too deep"); C->scope[C->nscope++] = C->nsym; }
static void pop_scope(void) { C->nsym = C->scope[--C->nscope]; }
static Sym *add_sym(const char *name, Ty t, const char *g, int kind)
{
    if (C->nsym == C->capsym) { C->capsym = C->capsym ? C->capsym * 2 : 128; C->syms = (Sym *)realloc(C->syms, (size_t)C->capsym * sizeof(Sym)); }
    Sym *s = &C->syms[C->nsym++]; memset(s, 0, sizeof(*s));
    s->name = adup(name); s->t = t; s->g = (char *)g; s->kind = kind; s->samp_slot = HLSL_NO_SLOT; return s;
}
static Sym *find_sym(const char *name)
{ for (int i = C->nsym - 1; i >= 0; --i) if (!strcmp(C->syms[i].name, name)) return &C->syms[i]; return NULL; }
static Struct *find_struct(const char *n)
{ for (int i = 0; i < C->nst; ++i) if (!strcmp(C->structs[i]->name, n)) return C->structs[i]; return NULL; }
static int find_typedef(const char *n, Ty *t)
{ for (int i = 0; i < C->ntd; ++i) if (!strcmp(C->typedefs[i].name, n)) { *t = C->typedefs[i].t; return 1; } return 0; }

/* ---- type-name parsing ------------------------------------------------------- */
static int scalar_word(const char *w, int *base)
{
    static const struct { const char *n; int b; } tab[] = {
        {"float",B_FLOAT},{"half",B_FLOAT},{"double",B_FLOAT},{"min16float",B_FLOAT},{"min10float",B_FLOAT},
        {"int",B_INT},{"min16int",B_INT},{"min12int",B_INT},{"uint",B_UINT},{"dword",B_UINT},{"min16uint",B_UINT},{"bool",B_BOOL},{0,0} };
    for (int i = 0; tab[i].n; ++i) if (!strcmp(w, tab[i].n)) { *base = tab[i].b; return 1; }
    return 0;
}
/* Recognises float, float3, float4x4, uint2, bool3 ... */
static int numeric_typename(const char *n, Ty *out)
{
    char w[24]; size_t i = 0;
    while (n[i] && !isdigit((unsigned char)n[i]) && i < sizeof(w) - 1) { w[i] = n[i]; ++i; }
    w[i] = 0;
    /* min16float2: the digits inside the word */
    if (!strncmp(w, "min", 3) && n[i]) { while (isdigit((unsigned char)n[i]) && i < sizeof(w) - 1) { w[i] = n[i]; ++i; } while (isalpha((unsigned char)n[i]) && i < sizeof(w) - 1) { w[i] = n[i]; ++i; } w[i] = 0; }
    int base; if (!scalar_word(w, &base)) return 0;
    const char *d = n + i;
    if (!*d) { *out = mkty(base, 1, 1); return 1; }
    if (isdigit((unsigned char)d[0]) && !d[1]) { int c = d[0] - '0'; if (c < 1 || c > 4) return 0; *out = mkty(base, 1, c); return 1; }
    if (isdigit((unsigned char)d[0]) && d[1] == 'x' && isdigit((unsigned char)d[2]) && !d[3]) {
        int r = d[0] - '0', c = d[2] - '0'; if (r < 1 || r > 4 || c < 1 || c > 4) return 0;
        if (base != B_FLOAT) return 0;
        *out = mkty(base, r, c); if (r == 1) { out->rows = 1; } return 1;
    }
    return 0;
}

/* ---- token helpers ------------------------------------------------------------- */
static Tok *cur(void) { return &C->toks[C->pos]; }
static Tok *peek(int n) { int p = C->pos + n; if (p >= C->ntok) p = C->ntok - 1; return &C->toks[p]; }
static int is_op(const char *s) { return cur()->k == T_OP && !strcmp(cur()->s, s); }
static int is_id(const char *s) { return cur()->k == T_ID && !strcmp(cur()->s, s); }
static int accept_op(const char *s) { if (is_op(s)) { C->pos++; return 1; } return 0; }
static int accept_id(const char *s) { if (is_id(s)) { C->pos++; return 1; } return 0; }
static void expect_op(const char *s) { if (!accept_op(s)) fail("expected '%s' but found '%s'", s, cur()->s); }
static char *expect_ident(void) { if (cur()->k != T_ID) fail("expected identifier but found '%s'", cur()->s); return C->toks[C->pos++].s; }

/* ---- conversions ------------------------------------------------------------------ */
static const char *zero_lit(int base) { return base == B_FLOAT ? "0.0" : base == B_UINT ? "0u" : base == B_INT ? "0" : "false"; }

static const char *swz_of(int n) { static const char *s[] = { "", "x", "xy", "xyz", "xyzw" }; return s[n]; }

/* Convert expression e (of type e.t) to type `to` following HLSL's implicit rules. */
static char *conv(Ex e, Ty to)
{
    Ty f = e.t;
    if (ty_eq(f, to)) return e.s;
    if (f.base == B_STRUCT || to.base == B_STRUCT || f.arr || to.arr) {
        if (f.base == B_STRUCT && to.base == B_STRUCT && f.st == to.st) return e.s;
        fail("cannot convert between these types");
    }
    if (!is_num(f) || !is_num(to)) fail("cannot convert non-numeric value");
    if (is_mat(to)) {
        if (!is_mat(f)) fail("cannot convert non-matrix to matrix");
        if (f.rows == to.rows && f.cols == to.cols) return e.s;              /* base conversions of matrices unsupported: float only */
        return S("%s(%s)", glsl_tyname(to), e.s);                               /* truncation / extension: top-left block */
    }
    if (is_mat(f)) fail("cannot convert matrix to vector/scalar");
    int fn = f.cols, tn = to.cols;
    const char *tname = glsl_tyname(to);
    if (to.base == B_BOOL && f.base != B_BOOL) {                              /* numeric -> bool: != 0 */
        char *src = fn > tn ? S("(%s).%s", e.s, swz_of(tn)) : e.s;
        int n = tn < fn ? tn : fn;
        if (n == 1) return S("(%s != %s)", src, zero_lit(f.base));
        return S("notEqual(%s, %s(%s))", src, glsl_tyname(with_cols(f, n)), zero_lit(f.base));
    }
    char *src = e.s;
    if (fn == 1 && tn > 1) {                                                   /* splat */
        if (f.base != to.base) return S("%s(%s)", tname, e.s);
        return S("%s(%s)", tname, e.s);
    }
    if (fn > tn) src = S("(%s).%s", e.s, swz_of(tn));                          /* truncate */
    else if (fn < tn) fail("cannot widen a %d-component value to %d components", fn, tn);
    if (f.base == to.base) return src;
    return S("%s(%s)", tname, src);                                            /* base change */
}
static char *to_bool(Ex e)
{
    if (!is_num(e.t) || e.t.arr) fail("condition must be numeric");
    if (e.t.base == B_BOOL && e.t.cols == 1 && e.t.rows == 1) return e.s;
    return conv(e, ty_b(1));
}
/* usual arithmetic conversion base: float > uint > int > bool(->int) */
static int promote_base(int a, int b)
{
    if (a == B_FLOAT || b == B_FLOAT) return B_FLOAT;
    if (a == B_UINT || b == B_UINT) return B_UINT;
    return B_INT;
}
