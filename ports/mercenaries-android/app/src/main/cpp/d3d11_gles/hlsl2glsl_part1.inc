/* ===== part 1: arena, strings, lexer, preprocessor ======================= */
#include "hlsl2glsl.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

typedef struct ArenaBlk { struct ArenaBlk *next; size_t used, cap; char data[]; } ArenaBlk;

typedef enum { T_EOF, T_ID, T_INT, T_UINT, T_FLT, T_OP } TokKind;
typedef struct { TokKind k; char *s; int line; } Tok;

typedef struct { char *name; int nparams; char *params[8]; Tok *body; int nbody; } Macro;

typedef struct Struct Struct;
typedef struct Ty {
    uint8_t base;     /* Base */
    uint8_t rows, cols;
    uint8_t texdim, texbase, texcols;
    uint16_t arr;
    Struct *st;
} Ty;

typedef struct { char *name; Ty t; char sem[32]; uint8_t flat, centroid; uint8_t rowmajor; } Field;
struct Struct { char *name; char *gname; Field *f; int nf, cap; };

typedef struct { char *name; Ty t; char *g; int kind; int slot; int samp_slot; int is_const; int is_static; int rowmajor; } Sym;
typedef struct { char *name; Ty ret; int np; Ty pt[16]; uint8_t pdir[16]; char *g; char *psem[16]; char retsem[32]; int is_entry; } Func;

typedef struct {
    ArenaBlk *arena;
    jmp_buf jb;
    char err[512];
    /* tokens */
    Tok *toks; int ntok, captok, pos;
    /* macros */
    Macro *macros; int nmac, capmac;
    /* symbols */
    Sym *syms; int nsym, capsym; int scope[64]; int nscope;
    Func *funcs; int nfunc, capfunc;
    Struct **structs; int nst, capst;
    struct { char *name; Ty t; } *typedefs; int ntd, captd;
    /* output pieces */
    char *decl, *funcs_out, *ginit;
    size_t ldecl, lfuncs, lginit, cdecl, cfuncs, cginit;
    int stage;
    int anon_counter;
    HlslResult *res;
    int tmp_counter;
    int cur_ret_valid; Ty cur_ret;
} Ctx;

static Ctx *C;

static void fail(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    char msg[400]; vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    int line = (C->pos < C->ntok && C->toks) ? C->toks[C->pos].line : 0;
    snprintf(C->err, sizeof(C->err), "line %d: %s", line, msg);
    longjmp(C->jb, 1);
}

static void *amalloc(size_t n)
{
    n = (n + 15) & ~(size_t)15;
    ArenaBlk *b = C->arena;
    if (!b || b->used + n > b->cap) {
        size_t cap = n > 65536 ? n : 65536;
        ArenaBlk *nb = (ArenaBlk *)malloc(sizeof(ArenaBlk) + cap);
        if (!nb) { snprintf(C->err, sizeof(C->err), "out of memory"); longjmp(C->jb, 1); }
        nb->next = C->arena; nb->used = 0; nb->cap = cap; C->arena = nb; b = nb;
    }
    void *p = b->data + b->used; b->used += n; return p;
}
static char *S(const char *fmt, ...)
{
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    char *s = (char *)amalloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, ap2); va_end(ap2);
    return s;
}
static char *adup(const char *s) { size_t n = strlen(s); char *d = (char *)amalloc(n + 1); memcpy(d, s, n + 1); return d; }

static void sappend(char **buf, size_t *len, size_t *cap, const char *s)
{
    size_t n = strlen(s);
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : 4096;
        while (nc < *len + n + 1) nc *= 2;
        char *nb = (char *)realloc(*buf, nc);
        if (!nb) { snprintf(C->err, sizeof(C->err), "out of memory"); longjmp(C->jb, 1); }
        *buf = nb; *cap = nc;
    }
    memcpy(*buf + *len, s, n + 1); *len += n;
}
#define EMIT_DECL(s)  sappend(&C->decl, &C->ldecl, &C->cdecl, (s))
#define EMIT_FUNC(s)  sappend(&C->funcs_out, &C->lfuncs, &C->cfuncs, (s))
#define EMIT_GINIT(s) sappend(&C->ginit, &C->lginit, &C->cginit, (s))

/* ---- token vector --------------------------------------------------------- */
typedef struct { Tok *v; int n, cap; } TokVec;
static void tv_push(TokVec *t, Tok k)
{
    if (t->n == t->cap) { t->cap = t->cap ? t->cap * 2 : 256; t->v = (Tok *)realloc(t->v, (size_t)t->cap * sizeof(Tok)); }
    t->v[t->n++] = k;
}

static const char *k_ops3[] = { "<<=", ">>=", NULL };
static const char *k_ops2[] = { "++", "--", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "==", "!=", "<=", ">=", "&&", "||", "<<", ">>", "::", NULL };

static char *norm_number(const char *start, size_t len, TokKind *kind)
{
    char buf[64]; if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, start, len); buf[len] = 0;
    int hex = (len > 1 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'));
    int isf = 0, isu = 0;
    if (!hex) { for (size_t i = 0; i < len; ++i) if (buf[i] == '.' || buf[i] == 'e' || buf[i] == 'E') isf = 1; }
    while (len && (buf[len-1] == 'f' || buf[len-1] == 'F' || buf[len-1] == 'u' || buf[len-1] == 'U' || buf[len-1] == 'l' || buf[len-1] == 'L')) {
        if (!hex && (buf[len-1] == 'f' || buf[len-1] == 'F')) isf = 1;
        else if (buf[len-1] == 'u' || buf[len-1] == 'U') isu = 1;
        else if (hex && (buf[len-1] == 'f' || buf[len-1] == 'F')) break;
        buf[--len] = 0;
    }
    if (isf && !hex) {
        double d = strtod(buf, NULL); char o[64]; snprintf(o, sizeof(o), "%.9g", d);
        if (!strpbrk(o, ".eEn")) strcat(o, ".0");
        *kind = T_FLT; return S("%s", o);
    }
    unsigned long long v = strtoull(buf, NULL, 0);
    *kind = isu ? T_UINT : T_INT;
    return isu ? S("%lluu", v) : S("%llu", v);
}

static void lex_line(const char *p, int line, TokVec *out)
{
    while (*p) {
        if (isspace((unsigned char)*p)) { ++p; continue; }
        Tok t; t.line = line;
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *s = p; while (isalnum((unsigned char)*p) || *p == '_') ++p;
            t.k = T_ID; t.s = S("%.*s", (int)(p - s), s);
        } else if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
            const char *s = p;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { p += 2; while (isxdigit((unsigned char)*p)) ++p; }
            else {
                while (isdigit((unsigned char)*p) || *p == '.') ++p;
                if (*p == 'e' || *p == 'E') { ++p; if (*p == '+' || *p == '-') ++p; while (isdigit((unsigned char)*p)) ++p; }
            }
            while (*p == 'f' || *p == 'F' || *p == 'u' || *p == 'U' || *p == 'l' || *p == 'L') ++p;
            t.s = norm_number(s, (size_t)(p - s), &t.k);
        } else {
            t.k = T_OP; int done = 0;
            for (int i = 0; k_ops3[i] && !done; ++i) if (!strncmp(p, k_ops3[i], 3)) { t.s = S("%s", k_ops3[i]); p += 3; done = 1; }
            for (int i = 0; k_ops2[i] && !done; ++i) if (!strncmp(p, k_ops2[i], 2)) { t.s = S("%s", k_ops2[i]); p += 2; done = 1; }
            if (!done) { t.s = S("%c", *p); ++p; }
        }
        tv_push(out, t);
    }
}

/* ---- macros ---------------------------------------------------------------- */
static Macro *find_macro(const char *n)
{ for (int i = 0; i < C->nmac; ++i) if (C->macros[i].name && !strcmp(C->macros[i].name, n)) return &C->macros[i]; return NULL; }

static void undef_macro(const char *n)
{ Macro *m = find_macro(n); if (m) m->name = NULL; }

static void add_macro(Macro m)
{
    undef_macro(m.name);
    if (C->nmac == C->capmac) { C->capmac = C->capmac ? C->capmac * 2 : 32; C->macros = (Macro *)realloc(C->macros, (size_t)C->capmac * sizeof(Macro)); }
    C->macros[C->nmac++] = m;
}

static void expand_tokens(const Tok *in, int n, TokVec *out, int depth, const char **hide, int nhide);

static void expand_one(const Tok *in, int n, int *i, TokVec *out, int depth, const char **hide, int nhide)
{
    const Tok *t = &in[*i];
    Macro *m = (t->k == T_ID && depth < 24) ? find_macro(t->s) : NULL;
    for (int h = 0; m && h < nhide; ++h) if (!strcmp(hide[h], t->s)) m = NULL;
    if (!m) { tv_push(out, *t); ++*i; return; }
    const char *nh[32]; int nn = nhide < 31 ? nhide : 31;
    memcpy(nh, hide, (size_t)nn * sizeof(char *)); nh[nn++] = m->name;
    if (m->nparams < 0) {
        TokVec sub = {0};
        expand_tokens(m->body, m->nbody, &sub, depth + 1, nh, nn);
        for (int k = 0; k < sub.n; ++k) { sub.v[k].line = t->line; tv_push(out, sub.v[k]); }
        free(sub.v); ++*i; return;
    }
    if (*i + 1 >= n || strcmp(in[*i + 1].s, "(") != 0) { tv_push(out, *t); ++*i; return; }
    int j = *i + 2, lvl = 1, np = 0;
    TokVec args[8]; memset(args, 0, sizeof(args));
    while (j < n && lvl > 0) {
        const Tok *a = &in[j];
        if (a->k == T_OP && !strcmp(a->s, "(")) ++lvl;
        else if (a->k == T_OP && !strcmp(a->s, ")")) { if (--lvl == 0) break; }
        if (lvl == 1 && a->k == T_OP && !strcmp(a->s, ",")) { if (++np >= 8) fail("too many macro args"); ++j; continue; }
        tv_push(&args[np], *a); ++j;
    }
    if (lvl != 0) fail("unterminated macro call %s", m->name);
    int argc = (args[0].n || np) ? np + 1 : (m->nparams == 0 ? 0 : 1);
    if (argc != m->nparams) fail("macro %s expects %d args", m->name, m->nparams);
    TokVec body = {0};
    for (int b = 0; b < m->nbody; ++b) {
        int pidx = -1;
        if (m->body[b].k == T_ID) for (int p = 0; p < m->nparams; ++p) if (!strcmp(m->params[p], m->body[b].s)) pidx = p;
        if (pidx >= 0) { TokVec ex = {0}; expand_tokens(args[pidx].v, args[pidx].n, &ex, depth + 1, nh, nn);
                         tv_push(&body, (Tok){ T_OP, (char *)"(", t->line }); for (int k = 0; k < ex.n; ++k) tv_push(&body, ex.v[k]);
                         tv_push(&body, (Tok){ T_OP, (char *)")", t->line }); free(ex.v); }
        else tv_push(&body, m->body[b]);
    }
    TokVec sub = {0};
    expand_tokens(body.v, body.n, &sub, depth + 1, nh, nn);
    for (int k = 0; k < sub.n; ++k) { sub.v[k].line = t->line; tv_push(out, sub.v[k]); }
    free(sub.v); free(body.v);
    for (int a = 0; a < 8; ++a) free(args[a].v);
    *i = j + 1;
}

static void expand_tokens(const Tok *in, int n, TokVec *out, int depth, const char **hide, int nhide)
{ for (int i = 0; i < n;) expand_one(in, n, &i, out, depth, hide, nhide); }

/* ---- #if expression --------------------------------------------------------- */
typedef struct { Tok *v; int n, p; } PP;
static long long pp_or(PP *);
static long long pp_prim(PP *x)
{
    if (x->p >= x->n) return 0;
    Tok t = x->v[x->p++];
    if (t.k == T_INT || t.k == T_UINT) return strtoll(t.s, NULL, 10);
    if (t.k == T_ID) return 0;
    if (t.k == T_OP && !strcmp(t.s, "(")) { long long v = pp_or(x); if (x->p < x->n) x->p++; return v; }
    if (t.k == T_OP && !strcmp(t.s, "!")) return !pp_prim(x);
    if (t.k == T_OP && !strcmp(t.s, "-")) return -pp_prim(x);
    return 0;
}
static long long pp_mul(PP *x) { long long a = pp_prim(x); while (x->p < x->n && x->v[x->p].k == T_OP && (x->v[x->p].s[0] == '*' || x->v[x->p].s[0] == '/' || x->v[x->p].s[0] == '%')) { char o = x->v[x->p++].s[0]; long long b = pp_prim(x); a = o == '*' ? a * b : (b ? (o == '/' ? a / b : a % b) : 0); } return a; }
static long long pp_add(PP *x) { long long a = pp_mul(x); while (x->p < x->n && x->v[x->p].k == T_OP && (x->v[x->p].s[0] == '+' || x->v[x->p].s[0] == '-') && !x->v[x->p].s[1]) { char o = x->v[x->p++].s[0]; long long b = pp_mul(x); a = o == '+' ? a + b : a - b; } return a; }
static long long pp_cmp(PP *x) { long long a = pp_add(x); while (x->p < x->n && x->v[x->p].k == T_OP && (!strcmp(x->v[x->p].s, "<") || !strcmp(x->v[x->p].s, ">") || !strcmp(x->v[x->p].s, "<=") || !strcmp(x->v[x->p].s, ">="))) { const char *o = x->v[x->p++].s; long long b = pp_add(x); a = !strcmp(o, "<") ? a < b : !strcmp(o, ">") ? a > b : !strcmp(o, "<=") ? a <= b : a >= b; } return a; }
static long long pp_eq(PP *x) { long long a = pp_cmp(x); while (x->p < x->n && x->v[x->p].k == T_OP && (!strcmp(x->v[x->p].s, "==") || !strcmp(x->v[x->p].s, "!="))) { int eq = x->v[x->p++].s[0] == '='; long long b = pp_cmp(x); a = eq ? a == b : a != b; } return a; }
static long long pp_and(PP *x) { long long a = pp_eq(x); while (x->p < x->n && x->v[x->p].k == T_OP && !strcmp(x->v[x->p].s, "&&")) { x->p++; long long b = pp_eq(x); a = a && b; } return a; }
static long long pp_or(PP *x) { long long a = pp_and(x); while (x->p < x->n && x->v[x->p].k == T_OP && !strcmp(x->v[x->p].s, "||")) { x->p++; long long b = pp_and(x); a = a || b; } return a; }

static long long eval_if(const Tok *in, int n)
{
    TokVec pre = {0};
    for (int i = 0; i < n; ++i) {   /* resolve defined() before expansion */
        if (in[i].k == T_ID && !strcmp(in[i].s, "defined")) {
            int j = i + 1; int paren = (j < n && !strcmp(in[j].s, "(")); if (paren) ++j;
            int d = (j < n && find_macro(in[j].s) != NULL);
            Tok t = { T_INT, (char *)(d ? "1" : "0"), in[i].line }; tv_push(&pre, t);
            i = j + (paren ? 1 : 0);
        } else tv_push(&pre, in[i]);
    }
    TokVec ex = {0}; expand_tokens(pre.v, pre.n, &ex, 0, NULL, 0);
    PP x = { ex.v, ex.n, 0 }; long long v = pp_or(&x);
    free(pre.v); free(ex.v); return v;
}

/* ---- full preprocess + lex ------------------------------------------------- */
static char *strip_comments(const char *src, size_t len)
{
    char *o = (char *)amalloc(len + 2); size_t j = 0;
    for (size_t i = 0; i < len;) {
        if (src[i] == '/' && i + 1 < len && src[i+1] == '/') { while (i < len && src[i] != '\n') ++i; }
        else if (src[i] == '/' && i + 1 < len && src[i+1] == '*') {
            i += 2; while (i + 1 < len && !(src[i] == '*' && src[i+1] == '/')) { if (src[i] == '\n') o[j++] = '\n'; ++i; }
            i += 2; o[j++] = ' ';
        } else if (src[i] == '\\' && i + 1 < len && src[i+1] == '\n') { i += 2; }
        else if (src[i] == '\\' && i + 2 < len && src[i+1] == '\r' && src[i+2] == '\n') { i += 3; }
        else o[j++] = src[i++];
    }
    o[j] = 0; return o;
}

static void preprocess(const char *src_in, size_t len)
{
    char *src = strip_comments(src_in, len);
    struct { int active, taken, parent; } st[32]; int sp = 0; int active = 1;
    TokVec all = {0}; int line = 0;
    for (char *p = src; *p;) {
        char *e = strchr(p, '\n'); size_t ll = e ? (size_t)(e - p) : strlen(p);
        char *ln = S("%.*s", (int)ll, p); ++line;
        p += ll + (e ? 1 : 0);
        char *q = ln; while (*q == ' ' || *q == '\t' || *q == '\r') ++q;
        if (*q == '#') {
            ++q; while (*q == ' ' || *q == '\t') ++q;
            char dir[16]; int dn = 0; while (isalpha((unsigned char)*q) && dn < 15) dir[dn++] = *q++; dir[dn] = 0;
            TokVec lt = {0}; lex_line(q, line, &lt);
            if (!strcmp(dir, "ifdef") || !strcmp(dir, "ifndef") || !strcmp(dir, "if")) {
                int cond = 0;
                if (active) {
                    if (!strcmp(dir, "if")) cond = eval_if(lt.v, lt.n) != 0;
                    else { int d = lt.n && find_macro(lt.v[0].s) != NULL; cond = !strcmp(dir, "ifdef") ? d : !d; }
                }
                if (sp >= 31) fail("#if nesting too deep");
                st[sp].parent = active; st[sp].taken = cond; st[sp].active = active && cond; ++sp; active = active && cond;
            } else if (!strcmp(dir, "elif")) {
                if (!sp) fail("#elif without #if");
                int cond = (!st[sp-1].taken && st[sp-1].parent) ? eval_if(lt.v, lt.n) != 0 : 0;
                if (cond) st[sp-1].taken = 1; active = st[sp-1].parent && cond;
            } else if (!strcmp(dir, "else")) {
                if (!sp) fail("#else without #if");
                active = st[sp-1].parent && !st[sp-1].taken; st[sp-1].taken = 1;
            } else if (!strcmp(dir, "endif")) {
                if (!sp) fail("#endif without #if");
                active = st[--sp].parent;
            } else if (active && !strcmp(dir, "define")) {
                if (!lt.n || lt.v[0].k != T_ID) fail("bad #define");
                Macro m; memset(&m, 0, sizeof(m)); m.name = lt.v[0].s; m.nparams = -1;
                int i = 1;
                /* function-like only if '(' directly follows the name */
                const char *after = strstr(q, m.name);
                if (after) after += strlen(m.name);
                if (lt.n > 1 && !strcmp(lt.v[1].s, "(") && after && *after == '(') {
                    m.nparams = 0; i = 2;
                    while (i < lt.n && strcmp(lt.v[i].s, ")")) { if (lt.v[i].k == T_ID) m.params[m.nparams++] = lt.v[i].s; ++i; }
                    ++i;
                }
                m.nbody = lt.n - i; m.body = (Tok *)amalloc((size_t)(m.nbody + 1) * sizeof(Tok));
                for (int k = 0; k < m.nbody; ++k) m.body[k] = lt.v[i + k];
                add_macro(m);
            } else if (active && !strcmp(dir, "undef")) { if (lt.n) undef_macro(lt.v[0].s); }
            else if (active && !strcmp(dir, "include")) fail("#include is not supported");
            free(lt.v);
            continue;
        }
        if (!active) continue;
        TokVec lt = {0}; lex_line(ln, line, &lt);
        TokVec ex = {0}; expand_tokens(lt.v, lt.n, &ex, 0, NULL, 0);
        for (int i = 0; i < ex.n; ++i) tv_push(&all, ex.v[i]);
        free(lt.v); free(ex.v);
    }
    if (sp) fail("unterminated #if");
    Tok eof = { T_EOF, (char *)"", line + 1 }; tv_push(&all, eof);
    C->toks = all.v; C->ntok = all.n; C->captok = all.cap; C->pos = 0;
}
