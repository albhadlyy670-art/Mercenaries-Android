/* ===== part 4: operators, statements, declarations ======================== */
typedef struct { char *p; size_t len, cap; } Sb;
static void sb_add(Sb *b, const char *s) { sappend(&b->p, &b->len, &b->cap, s); }
static void sb_free(Sb *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }

static int binprec(const char *op)
{
    if (!strcmp(op, "||")) return 1; if (!strcmp(op, "&&")) return 2; if (!strcmp(op, "|")) return 3;
    if (!strcmp(op, "^")) return 4; if (!strcmp(op, "&")) return 5;
    if (!strcmp(op, "==") || !strcmp(op, "!=")) return 6;
    if (!strcmp(op, "<") || !strcmp(op, ">") || !strcmp(op, "<=") || !strcmp(op, ">=")) return 7;
    if (!strcmp(op, "<<") || !strcmp(op, ">>")) return 8;
    if (!strcmp(op, "+") || !strcmp(op, "-")) return 9;
    if (!strcmp(op, "*") || !strcmp(op, "/") || !strcmp(op, "%")) return 10;
    return 0;
}

static Ex binop(const char *op, Ex a, Ex b)
{
    if (!strcmp(op, "&&") || !strcmp(op, "||")) return mk(S("(%s %s %s)", to_bool(a), op, to_bool(b)), ty_b(1));
    if ((a.t.base == B_STRUCT) || (b.t.base == B_STRUCT)) fail("operator '%s' on a struct", op);
    int cmp = !strcmp(op, "<") || !strcmp(op, ">") || !strcmp(op, "<=") || !strcmp(op, ">=") || !strcmp(op, "==") || !strcmp(op, "!=");
    int bit = !strcmp(op, "&") || !strcmp(op, "|") || !strcmp(op, "^");
    int shift = !strcmp(op, "<<") || !strcmp(op, ">>");
    if (bit && a.t.base == B_BOOL && b.t.base == B_BOOL && is_scalar(a.t) && is_scalar(b.t))
        return mk(S("(%s %s %s)", a.s, op[0] == '&' ? "&&" : op[0] == '|' ? "||" : "!=", b.s), ty_b(1));
    if (shift) {
        Ty lt = a.t.base == B_UINT ? a.t : (a.t.base == B_INT ? a.t : with_base(a.t, B_INT));
        Ty rt = with_base(b.t, B_INT); if (b.t.base == B_UINT) rt = b.t;
        if (rt.cols != lt.cols) rt = with_cols(rt, lt.cols == 1 ? 1 : lt.cols);
        return mk(S("(%s %s %s)", conv(a, lt), op, conv(b, rt)), lt);
    }
    Ty T = common2(a.t, b.t);
    if (cmp && a.t.base == B_BOOL && b.t.base == B_BOOL) T = with_cols(ty_b(1), T.cols);
    if (bit) { if (T.base == B_FLOAT || T.base == B_BOOL) T = with_base(T, B_INT); }
    Ty ta = (is_scalar(a.t) && !is_scalar(T) && !is_mat(T)) ? with_cols(T, 1) : T;
    Ty tb = (is_scalar(b.t) && !is_scalar(T) && !is_mat(T)) ? with_cols(T, 1) : T;
    if (is_mat(T)) { if (is_scalar(a.t)) ta = ty_f(1); if (is_scalar(b.t)) tb = ty_f(1); }
    char *x = conv(a, ta), *y = conv(b, tb);
    if (cmp) {
        if (T.cols == 1 && !is_mat(T)) return mk(S("(%s %s %s)", x, op, y), ty_b(1));
        const char *fn = !strcmp(op, "<") ? "lessThan" : !strcmp(op, ">") ? "greaterThan" : !strcmp(op, "<=") ? "lessThanEqual" :
                         !strcmp(op, ">=") ? "greaterThanEqual" : !strcmp(op, "==") ? "equal" : "notEqual";
        if (is_scalar(a.t)) x = conv(a, T); if (is_scalar(b.t)) y = conv(b, T);
        return mk(S("%s(%s, %s)", fn, x, y), with_base(T, B_BOOL));
    }
    if (!strcmp(op, "%") && T.base == B_FLOAT) { g_need_fmod = 1; return mk(S("(%s - %s * trunc(%s / %s))", paren(x), paren(y), paren(x), paren(y)), T); }
    if (!strcmp(op, "*") && is_mat(a.t) && is_mat(b.t)) return mk(S("matrixCompMult(%s, %s)", x, y), T);
    return mk(S("(%s %s %s)", x, op, y), T);
}

static Ex cast_to(Ty to, Ex e)
{
    if (is_mat(to) && is_scalar(e.t)) { Ex a[1] = { e }; return make_ctor(to, a, 1); }
    if (to.base == B_STRUCT) return e;
    return mk(conv(e, to), to);
}

static Ex parse_unary(void)
{
    if (looks_like_cast()) {
        C->pos++; Ty to = parse_type(); expect_op(")"); Ex e = parse_unary(); return cast_to(to, e);
    }
    Tok *t = cur();
    if (t->k == T_OP) {
        if (!strcmp(t->s, "-")) { C->pos++; Ex e = parse_unary(); return mk(S("(-%s)", paren(e.s)), e.t); }
        if (!strcmp(t->s, "+")) { C->pos++; return parse_unary(); }
        if (!strcmp(t->s, "!")) { C->pos++; Ex e = parse_unary();
            if (is_scalar(e.t)) return mk(S("(!%s)", paren(to_bool(e))), ty_b(1));
            return mk(S("not(%s)", conv(e, with_base(e.t, B_BOOL))), with_base(e.t, B_BOOL)); }
        if (!strcmp(t->s, "~")) { C->pos++; Ex e = parse_unary(); return mk(S("(~%s)", paren(e.s)), e.t); }
        if (!strcmp(t->s, "++") || !strcmp(t->s, "--")) { C->pos++; Ex e = parse_unary(); return mk(S("(%s%s)", t->s, paren(e.s)), e.t); }
    }
    return parse_postfix();
}

static Ex parse_binary(int minprec)
{
    Ex lhs = parse_unary();
    for (;;) {
        Tok *t = cur(); if (t->k != T_OP) break;
        int p = binprec(t->s); if (!p || p < minprec) break;
        char *op = t->s; C->pos++;
        Ex rhs = parse_binary(p + 1);
        lhs = binop(op, lhs, rhs);
    }
    return lhs;
}

static Ex parse_ternary(void)
{
    Ex c = parse_binary(1);
    if (!is_op("?")) return c;
    C->pos++; Ex a = parse_assign(); expect_op(":"); Ex b = parse_ternary();
    if (!is_scalar(c.t) && !(c.t.base == B_BOOL && c.t.cols == 1)) fail("vector conditions in ?: are not supported");
    char *cond = to_bool(c);
    if (a.t.base == B_STRUCT || b.t.base == B_STRUCT || a.t.arr) return mk(S("(%s ? %s : %s)", cond, a.s, b.s), a.t);
    Ty T = common2(a.t, b.t);
    if (a.t.base == B_BOOL && b.t.base == B_BOOL) T = with_cols(ty_b(1), T.cols);
    return mk(S("(%s ? %s : %s)", cond, conv(a, T), conv(b, T)), T);
}

static Ex parse_assign(void)
{
    Ex l = parse_ternary();
    Tok *t = cur();
    if (t->k == T_OP && (!strcmp(t->s, "=") || !strcmp(t->s, "+=") || !strcmp(t->s, "-=") || !strcmp(t->s, "*=") || !strcmp(t->s, "/=") ||
        !strcmp(t->s, "%=") || !strcmp(t->s, "&=") || !strcmp(t->s, "|=") || !strcmp(t->s, "^=") || !strcmp(t->s, "<<=") || !strcmp(t->s, ">>="))) {
        char *op = t->s; C->pos++;
        Ex r = parse_assign();
        if (!strcmp(op, "=")) return mk(S("%s = %s", l.s, conv(r, l.t)), l.t);
        if (!strcmp(op, "%=") && l.t.base == B_FLOAT) { g_need_fmod = 1; char *lr = conv(r, l.t); return mk(S("%s = (%s - %s * trunc(%s / %s))", l.s, l.s, paren(lr), l.s, paren(lr)), l.t); }
        if (!strcmp(op, "*=") && is_mat(l.t) && is_mat(r.t)) return mk(S("%s = matrixCompMult(%s, %s)", l.s, l.s, r.s), l.t);
        Ty rt = (is_scalar(r.t) && !is_scalar(l.t) && !is_mat(l.t)) ? with_base(ty_f(1), l.t.base) : l.t;
        if (is_scalar(r.t) && is_mat(l.t)) rt = ty_f(1);
        return mk(S("%s %s %s", l.s, op, conv(r, rt)), l.t);
    }
    return l;
}

static Ex parse_expr_comma(void)
{
    Ex e = parse_assign();
    while (is_op(",")) { C->pos++; Ex r = parse_assign(); e = mk(S("(%s, %s)", e.s, r.s), r.t); }
    return e;
}

/* ---- constant helpers ---------------------------------------------------------- */
static int parse_const_int(void)
{
    Ex e = parse_ternary();
    char *end; long v = strtol(e.s, &end, 10);
    if (*end == 'u') ++end;
    if (end == e.s || (*end && *end != 0)) {
        /* simple parenthesised literal such as (4) */
        const char *p = e.s; while (*p == '(') ++p; v = strtol(p, &end, 10);
        if (end == p) fail("array size must be a constant integer");
    }
    return (int)v;
}

static const char *k_ctor_names[] = { "float","int","uint","bool","vec2","vec3","vec4","ivec2","ivec3","ivec4","uvec2","uvec3","uvec4","bvec2","bvec3","bvec4","mat2","mat3","mat4","transpose", NULL };
static int is_const_expr(const char *s)
{
    for (const char *p = s; *p;) {
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *b = p; while (isalnum((unsigned char)*p) || *p == '_') ++p;
            size_t n = (size_t)(p - b); int ok = 0;
            for (int i = 0; k_ctor_names[i]; ++i) if (strlen(k_ctor_names[i]) == n && !strncmp(b, k_ctor_names[i], n)) ok = 1;
            if (!ok) { for (int i = 0; i < C->nsym; ++i) if (C->syms[i].is_const && C->syms[i].g && strlen(C->syms[i].g) == n && !strncmp(C->syms[i].g, b, n)) ok = 1; }
            if (!ok && (!strncmp(b, "true", n) || !strncmp(b, "false", n))) ok = (n == 4 || n == 5);
            if (!ok) return 0;
        } else ++p;
    }
    return 1;
}

static char *parse_initializer(Ty t);
static char *init_list(Ty t)
{
    expect_op("{");
    Ex elems[256]; char *strs[256]; int n = 0;
    Ty et = t; if (t.arr) et.arr = 0;
    while (!is_op("}")) {
        if (n >= 256) fail("initializer list too long");
        if (is_op("{")) strs[n++] = init_list(t.arr ? et : t);
        else {
            Ex e = parse_assign();
            if (t.arr) strs[n++] = conv(e, et);
            else if (t.base == B_STRUCT) { Field *f = &t.st->f[n < t.st->nf ? n : 0]; strs[n++] = conv(e, f->t); }
            else { elems[n] = e; strs[n++] = e.s; }
        }
        if (!accept_op(",")) break;
    }
    expect_op("}");
    char *args = ""; for (int i = 0; i < n; ++i) args = S("%s%s%s", args, i ? ", " : "", strs[i]);
    if (t.arr) return S("%s[%d](%s)", glsl_tyname(et), t.arr, args);
    if (t.base == B_STRUCT) return S("%s(%s)", t.st->gname, args);
    if (is_mat(t)) return S("transpose(%s(%s))", glsl_tyname(mkty(B_FLOAT, t.cols, t.rows)), args);
    return S("%s(%s)", glsl_tyname(t), args);
}
static char *parse_initializer(Ty t)
{
    if (is_op("{")) return init_list(t);
    Ex e = parse_assign();
    return conv(e, t);
}

/* ---- local declarations ---------------------------------------------------------- */
static void parse_semantic_opt(char *sem, size_t n)
{
    sem[0] = 0;
    if (is_op(":")) {
        Tok *nx = peek(1);
        if (nx->k == T_ID && (!strcmp(nx->s, "register") || !strcmp(nx->s, "packoffset"))) return;
        C->pos++; snprintf(sem, n, "%s", expect_ident());
    }
}
static void skip_register_or_pack(int *reg_kind, int *reg_num)
{
    if (is_op(":") && peek(1)->k == T_ID && (!strcmp(peek(1)->s, "register") || !strcmp(peek(1)->s, "packoffset"))) {
        int isreg = !strcmp(peek(1)->s, "register");
        C->pos += 2; expect_op("(");
        int depth = 1; int first = 1;
        while (depth > 0 && cur()->k != T_EOF) {
            if (is_op("(")) depth++; else if (is_op(")")) { if (--depth == 0) break; }
            else if (isreg && first && cur()->k == T_ID && strlen(cur()->s) >= 2 && strchr("btsu", cur()->s[0]) && isdigit((unsigned char)cur()->s[1])) {
                if (reg_kind) *reg_kind = cur()->s[0]; if (reg_num) *reg_num = atoi(cur()->s + 1); first = 0;
            }
            C->pos++;
        }
        expect_op(")");
    }
}

static void parse_local_decl(Sb *o, int in_for)
{
    Quals q; memset(&q, 0, sizeof(q)); parse_quals(&q);
    Ty base = parse_type();
    int first = 1;
    do {
        char *name = expect_ident(); Ty t = base;
        if (is_op("[")) { C->pos++; t.arr = (uint16_t)parse_const_int(); expect_op("]"); }
        char sem[32]; parse_semantic_opt(sem, sizeof(sem));
        char *g = gid(name);
        if (t.base == B_TEX || t.base == B_SAMP) fail("local texture/sampler variables are not supported");
        char *init = NULL;
        if (accept_op("=")) init = parse_initializer(t);
        if (in_for && !first) sb_add(o, ", ");
        if (in_for && first) sb_add(o, glsl_tyname(with_cols(t, t.cols)) ? "" : "");
        char *decl = (in_for && !first) ? S("%s%s%s", g, t.arr ? S("[%d]", t.arr) : "", init ? S(" = %s", init) : "")
                                        : S("%s%s", glsl_decl(t, g), init ? S(" = %s", init) : "");
        sb_add(o, decl);
        if (!in_for) sb_add(o, ";\n");
        add_sym(name, t, g, SYM_LOCAL);
        first = 0;
    } while (accept_op(","));
    if (!in_for) expect_op(";");
}

static void parse_statement(Sb *o);
static void parse_block_body(Sb *o)
{
    expect_op("{"); push_scope(); sb_add(o, "{\n");
    while (!is_op("}")) { if (cur()->k == T_EOF) fail("unterminated block"); parse_statement(o); }
    expect_op("}"); pop_scope(); sb_add(o, "}\n");
}
static void parse_substatement(Sb *o)
{
    if (is_op("{")) { parse_block_body(o); return; }
    sb_add(o, "{\n"); push_scope(); parse_statement(o); pop_scope(); sb_add(o, "}\n");
}
static int at_decl_start(void)
{
    int save = C->pos; Quals q; memset(&q, 0, sizeof(q)); parse_quals(&q);
    int r = at_type_name(cur()) && !(peek(1)->k == T_OP && !strcmp(peek(1)->s, "(") && !find_struct(cur()->s) ? 0 : 0);
    /* a type name followed by '(' is a constructor call expression, not a declaration */
    if (r && peek(1)->k == T_OP && !strcmp(peek(1)->s, "(")) r = 0;
    C->pos = save; return r;
}

static void parse_statement(Sb *o)
{
    Tok *t = cur();
    if (t->k == T_OP && !strcmp(t->s, ";")) { C->pos++; sb_add(o, ";\n"); return; }
    if (t->k == T_OP && !strcmp(t->s, "{")) { parse_block_body(o); return; }
    if (t->k == T_OP && !strcmp(t->s, "[")) {                      /* attributes: [unroll] [branch] [loop] [flatten] */
        int d = 0; do { if (is_op("[")) d++; else if (is_op("]")) d--; C->pos++; } while (d > 0 && cur()->k != T_EOF);
        parse_statement(o); return;
    }
    if (t->k == T_ID) {
        const char *w = t->s;
        if (!strcmp(w, "if")) {
            C->pos++; expect_op("("); Ex c = parse_expr_comma(); expect_op(")");
            sb_add(o, S("if (%s)\n", to_bool(c))); parse_substatement(o);
            if (accept_id("else")) { sb_add(o, "else\n"); if (is_id("if")) { sb_add(o, "{\n"); parse_statement(o); sb_add(o, "}\n"); } else parse_substatement(o); }
            return;
        }
        if (!strcmp(w, "for")) {
            C->pos++; expect_op("("); push_scope();
            Sb init = {0};
            if (!is_op(";")) { if (at_decl_start()) parse_local_decl(&init, 1); else { Ex e = parse_expr_comma(); sb_add(&init, e.s); } }
            expect_op(";");
            char *cond = ""; if (!is_op(";")) { Ex c = parse_expr_comma(); cond = to_bool(c); }
            expect_op(";");
            char *iter = ""; if (!is_op(")")) { Ex e = parse_expr_comma(); iter = e.s; }
            expect_op(")");
            sb_add(o, S("for (%s; %s; %s)\n", init.p ? init.p : "", cond, iter)); sb_free(&init);
            parse_substatement(o); pop_scope(); return;
        }
        if (!strcmp(w, "while")) {
            C->pos++; expect_op("("); Ex c = parse_expr_comma(); expect_op(")");
            sb_add(o, S("while (%s)\n", to_bool(c))); parse_substatement(o); return;
        }
        if (!strcmp(w, "do")) {
            C->pos++; sb_add(o, "do\n"); parse_substatement(o);
            if (!accept_id("while")) fail("expected 'while' after do-body");
            expect_op("("); Ex c = parse_expr_comma(); expect_op(")"); expect_op(";");
            sb_add(o, S("while (%s);\n", to_bool(c))); return;
        }
        if (!strcmp(w, "switch")) {
            C->pos++; expect_op("("); Ex c = parse_expr_comma(); expect_op(")");
            Ty it = c.t.base == B_UINT ? ty_u(1) : ty_i(1);
            sb_add(o, S("switch (%s)\n", conv(c, it))); parse_block_body(o); return;
        }
        if (!strcmp(w, "case")) { C->pos++; Ex e = parse_ternary(); expect_op(":"); sb_add(o, S("case %s:\n", e.s)); return; }
        if (!strcmp(w, "default") && peek(1)->k == T_OP && !strcmp(peek(1)->s, ":")) { C->pos += 2; sb_add(o, "default:\n"); return; }
        if (!strcmp(w, "break")) { C->pos++; expect_op(";"); sb_add(o, "break;\n"); return; }
        if (!strcmp(w, "continue")) { C->pos++; expect_op(";"); sb_add(o, "continue;\n"); return; }
        if (!strcmp(w, "discard")) { C->pos++; expect_op(";"); sb_add(o, "discard;\n"); return; }
        if (!strcmp(w, "return")) {
            C->pos++;
            if (accept_op(";")) { sb_add(o, "return;\n"); return; }
            Ex e = parse_expr_comma(); expect_op(";");
            if (C->cur_ret.base == B_VOID) fail("return with a value in a void function");
            sb_add(o, S("return %s;\n", conv(e, C->cur_ret))); return;
        }
        if (at_decl_start()) { parse_local_decl(o, 0); return; }
    }
    Ex e = parse_expr_comma(); expect_op(";");
    if (e.stmt) { sb_add(o, e.s); sb_add(o, "\n"); } else sb_add(o, S("%s;\n", e.s));
}
