/* winapi_extra.c - POSIX implementations of the Win32 functions the game uses that win32_compat.c lacks. */
#include <windows.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

/* ---- MSVC "secure" string functions -------------------------------------------------------- */
errno_t strcpy_s(char *dst, size_t size, const char *src)
{
    if (!dst || !size) return EINVAL;
    if (!src) { dst[0] = 0; return EINVAL; }
    size_t n = strlen(src);
    if (n >= size) { dst[0] = 0; return ERANGE; }
    memcpy(dst, src, n + 1); return 0;
}
errno_t strcat_s(char *dst, size_t size, const char *src)
{
    if (!dst || !size) return EINVAL;
    size_t d = strnlen(dst, size);
    if (d == size) return EINVAL;
    if (!src) { dst[0] = 0; return EINVAL; }
    size_t n = strlen(src);
    if (d + n >= size) { dst[0] = 0; return ERANGE; }
    memcpy(dst + d, src, n + 1); return 0;
}
errno_t strncpy_s(char *dst, size_t size, const char *src, size_t count)
{
    if (!dst || !size) return EINVAL;
    if (!src) { dst[0] = 0; return EINVAL; }
    size_t n = strnlen(src, count == (size_t)-1 ? (size_t)-1 : count);
    if (n >= size) {
        if (count == (size_t)-1) { memcpy(dst, src, size - 1); dst[size - 1] = 0; return STRUNCATE; }
        dst[0] = 0; return ERANGE;
    }
    memcpy(dst, src, n); dst[n] = 0; return 0;
}
errno_t freopen_s(FILE **out, const char *path, const char *mode, FILE *stream)
{ FILE *f = freopen(path, mode, stream); if (out) *out = f; return f ? 0 : (errno ? errno : EIO); }

/* ---- INI files (GetPrivateProfile* / WritePrivateProfileString) ------------------------------ */
typedef struct { char *text; size_t len; } IniFile;
static void ini_load(const char *path, IniFile *f)
{
    f->text = NULL; f->len = 0; FILE *fp = path ? fopen(path, "rb") : NULL; if (!fp) return;
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    if (n > 0 && n < (16 << 20)) { f->text = (char *)malloc((size_t)n + 1); if (f->text && fread(f->text, 1, (size_t)n, fp) == (size_t)n) { f->len = (size_t)n; f->text[n] = 0; } else { free(f->text); f->text = NULL; } }
    fclose(fp);
}
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') ++s;
    char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}
/* Iterates logical lines. Returns 0 at end. `line` receives a writable copy. */
typedef struct { const char *p, *end; } LineIter;
static int next_line(LineIter *it, char *line, size_t cap, size_t *start, size_t *stop, const char *base)
{
    if (it->p >= it->end) return 0;
    const char *e = memchr(it->p, '\n', (size_t)(it->end - it->p)); const char *nx = e ? e + 1 : it->end; if (!e) e = it->end;
    size_t n = (size_t)(e - it->p); if (n >= cap) n = cap - 1;
    memcpy(line, it->p, n); line[n] = 0;
    *start = (size_t)(it->p - base); *stop = (size_t)(nx - base); it->p = nx; return 1;
}
/* Parses one line: sets *sect (section header text) or key/value. Returns 1 header, 2 key=value, 0 other. */
static int parse_line(char *line, char **a, char **b)
{
    char *s = trim(line); if (!*s || *s == ';' || *s == '#') return 0;
    if (*s == '[') { char *r = strchr(s, ']'); if (!r) return 0; *r = 0; *a = trim(s + 1); return 1; }
    char *eq = strchr(s, '='); if (!eq) return 0; *eq = 0; *a = trim(s); *b = trim(eq + 1);
    /* Windows strips a matching pair of surrounding quotes from values */
    size_t n = strlen(*b); if (n >= 2 && (*b)[0] == '"' && (*b)[n - 1] == '"') { (*b)[n - 1] = 0; ++*b; }
    return 2;
}
/* Find key in section; returns 1 and fills value, plus byte range of the line. */
static int ini_find(const IniFile *f, const char *section, const char *key, char *value, size_t vcap, size_t *ls, size_t *le, size_t *sec_end, int *sec_found)
{
    LineIter it = { f->text, f->text ? f->text + f->len : NULL }; char line[2048]; size_t s, e; int in_sec = 0; *sec_found = 0; *sec_end = f->len; int found = 0;
    if (!f->text) return 0;
    while (next_line(&it, line, sizeof(line), &s, &e, f->text)) {
        char *a = NULL, *b = NULL; int kind = parse_line(line, &a, &b);
        if (kind == 1) { if (in_sec) { *sec_end = s; in_sec = 0; } if (!strcasecmp(a, section)) { in_sec = 1; *sec_found = 1; *sec_end = f->len; } }
        else if (kind == 2 && in_sec && !found && key && !strcasecmp(a, key)) { found = 1; *ls = s; *le = e; if (value) { snprintf(value, vcap, "%s", b); } }
        if (in_sec) *sec_end = e;      /* section runs to the end of its last line so far */
    }
    return found;
}
DWORD GetPrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR def, LPSTR out, DWORD size, LPCSTR path)
{
    if (!out || !size) return 0;
    IniFile f; ini_load(path, &f); DWORD n = 0;
    if (section && key) {
        char value[2048]; size_t ls, le, se; int sf;
        const char *r = ini_find(&f, section, key, value, sizeof(value), &ls, &le, &se, &sf) ? value : (def ? def : "");
        snprintf(out, size, "%s", r); n = (DWORD)strlen(out);
    } else if (f.text) {                                   /* enumerate section names or key names */
        LineIter it = { f.text, f.text + f.len }; char line[2048]; size_t s, e; int in_sec = 0; DWORD pos = 0;
        while (next_line(&it, line, sizeof(line), &s, &e, f.text)) {
            char *a = NULL, *b = NULL; int kind = parse_line(line, &a, &b);
            if (kind == 1) { in_sec = section && !strcasecmp(a, section); if (!section) { size_t l = strlen(a); if (pos + l + 2 < size) { memcpy(out + pos, a, l + 1); pos += (DWORD)l + 1; } } }
            else if (kind == 2 && in_sec && section) { size_t l = strlen(a); if (pos + l + 2 < size) { memcpy(out + pos, a, l + 1); pos += (DWORD)l + 1; } }
        }
        out[pos] = 0; out[pos + 1 < size ? pos + 1 : pos] = 0; n = pos;
    } else { out[0] = 0; if (size > 1) out[1] = 0; }
    free(f.text); return n;
}
DWORD GetPrivateProfileIntA(LPCSTR section, LPCSTR key, INT def, LPCSTR path)
{
    char v[64]; IniFile f; ini_load(path, &f); size_t ls, le, se; int sf; int has = ini_find(&f, section, key, v, sizeof(v), &ls, &le, &se, &sf); free(f.text);
    if (!has) return (DWORD)def;
    char *end; long long x = strtoll(v, &end, 10); if (end == v) return (DWORD)def; return (DWORD)x;
}
DWORD GetPrivateProfileSectionA(LPCSTR section, LPSTR out, DWORD size, LPCSTR path)
{
    if (!out || size < 2) return 0;
    IniFile f; ini_load(path, &f); DWORD pos = 0; out[0] = out[1] = 0;
    if (f.text) {
        LineIter it = { f.text, f.text + f.len }; char line[2048]; size_t s, e; int in_sec = 0;
        while (next_line(&it, line, sizeof(line), &s, &e, f.text)) {
            char raw[2048]; snprintf(raw, sizeof(raw), "%s", line); char *a = NULL, *b = NULL; int kind = parse_line(line, &a, &b);
            if (kind == 1) in_sec = !strcasecmp(a, section);
            else if (kind == 2 && in_sec) { char *t = trim(raw); size_t l = strlen(t); if (pos + l + 2 >= size) break; memcpy(out + pos, t, l + 1); pos += (DWORD)l + 1; }
        }
        out[pos] = 0; if (pos + 1 < size) out[pos + 1] = 0;
    }
    free(f.text); return pos;
}
BOOL WritePrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR path)
{
    if (!section || !path) return FALSE;
    IniFile f; ini_load(path, &f); size_t ls = 0, le = 0, se = 0; int sf = 0; char tmp[2048];
    int has = key ? ini_find(&f, section, key, tmp, sizeof(tmp), &ls, &le, &se, &sf) : 0;
    if (!key) ini_find(&f, section, "", NULL, 0, &ls, &le, &se, &sf);
    char *buf = (char *)malloc(f.len + strlen(section) + (key ? strlen(key) : 0) + (value ? strlen(value) : 0) + 64); if (!buf) { free(f.text); return FALSE; }
    size_t n = 0;
    if (!key) {                                                           /* delete the whole section */
        if (sf) {
            LineIter it = { f.text, f.text + f.len }; char line[2048]; size_t s, e; int in_sec = 0;
            while (next_line(&it, line, sizeof(line), &s, &e, f.text)) {
                char *a = NULL, *b = NULL; int kind = parse_line(line, &a, &b); if (kind == 1) in_sec = !strcasecmp(a, section);
                if (!in_sec) { memcpy(buf + n, f.text + s, e - s); n += e - s; }
            }
        } else if (f.text) { memcpy(buf, f.text, f.len); n = f.len; }
    } else if (has) {                                                     /* replace or delete the key line */
        memcpy(buf, f.text, ls); n = ls;
        if (value) n += (size_t)sprintf(buf + n, "%s=%s\r\n", key, value);
        memcpy(buf + n, f.text + le, f.len - le); n += f.len - le;
    } else if (value) {                                                   /* insert key */
        if (sf) {
            memcpy(buf, f.text, se); n = se; if (n && buf[n - 1] != '\n') n += (size_t)sprintf(buf + n, "\r\n");
            n += (size_t)sprintf(buf + n, "%s=%s\r\n", key, value); memcpy(buf + n, f.text + se, f.len - se); n += f.len - se;
        } else {
            if (f.len) { memcpy(buf, f.text, f.len); n = f.len; if (buf[n - 1] != '\n') n += (size_t)sprintf(buf + n, "\r\n"); }
            n += (size_t)sprintf(buf + n, "[%s]\r\n%s=%s\r\n", section, key, value);
        }
    } else if (f.text) { memcpy(buf, f.text, f.len); n = f.len; }
    char tmpname[1100]; snprintf(tmpname, sizeof(tmpname), "%s.tmp%d", path, (int)getpid());
    FILE *fp = fopen(tmpname, "wb"); BOOL ok = FALSE;
    if (fp) { ok = fwrite(buf, 1, n, fp) == n; ok = (fclose(fp) == 0) && ok; if (ok) ok = rename(tmpname, path) == 0; if (!ok) unlink(tmpname); }
    free(buf); free(f.text); return ok;
}

/* ---- files and modules --------------------------------------------------------------------------- */
DWORD GetFileAttributesA(LPCSTR path)
{ struct stat st; if (!path || stat(path, &st) != 0) return INVALID_FILE_ATTRIBUTES; return S_ISDIR(st.st_mode) ? 0x10u : 0x80u; }
DWORD GetFullPathNameA(LPCSTR name, DWORD size, LPSTR out, LPSTR *file_part)
{
    char joined[4096], norm[4096]; size_t j = 0;
    if (!name) return 0;
    if (name[0] != '/' && name[0] != '\\') { if (!getcwd(joined, sizeof(joined) - 2)) return 0; j = strlen(joined); joined[j++] = '/'; }
    for (const char *p = name; *p && j < sizeof(joined) - 1; ++p) joined[j++] = *p == '\\' ? '/' : *p;
    joined[j] = 0;
    size_t n = 0; char *save = NULL; norm[0] = 0;
    for (char *tok = strtok_r(joined, "/", &save); tok; tok = strtok_r(NULL, "/", &save)) {
        if (!strcmp(tok, ".") || !*tok) continue;
        if (!strcmp(tok, "..")) { while (n > 0 && norm[n - 1] != '/') --n; if (n > 0) --n; norm[n] = 0; continue; }
        n += (size_t)snprintf(norm + n, sizeof(norm) - n, "/%s", tok);
    }
    if (!n) { norm[0] = '/'; norm[1] = 0; n = 1; }
    if (n + 1 > size) return (DWORD)(n + 1);
    memcpy(out, norm, n + 1); if (file_part) { char *s = strrchr(out, '/'); *file_part = s && s[1] ? s + 1 : NULL; }
    return (DWORD)n;
}
DWORD GetModuleFileNameA(HMODULE module, LPSTR out, DWORD size)
{
    (void)module; const char *p = getenv("MERCENARIES_EXE_PATH");
    if (p) { snprintf(out, size, "%s", p); return (DWORD)strlen(out); }
    ssize_t n = readlink("/proc/self/exe", out, size ? size - 1 : 0); if (n < 0) { if (size) out[0] = 0; return 0; }
    out[n] = 0; return (DWORD)n;
}
HMODULE GetModuleHandleA(LPCSTR name) { (void)name; return (HMODULE)(uintptr_t)1; }
HMODULE GetModuleHandleW(const WCHAR *name) { (void)name; return (HMODULE)(uintptr_t)1; }
void *GetProcAddress(HMODULE module, LPCSTR name) { (void)module; (void)name; return NULL; }   /* optional Windows-only entry points */

/* ---- atomics / diagnostics / window stubs ---------------------------------------------------------------- */
LONG InterlockedAnd(volatile LONG *p, LONG v) { return __atomic_fetch_and(p, v, __ATOMIC_SEQ_CST); }
LONG InterlockedOr(volatile LONG *p, LONG v) { return __atomic_fetch_or(p, v, __ATOMIC_SEQ_CST); }
LONG64 InterlockedCompareExchange64(volatile LONG64 *p, LONG64 x, LONG64 c) { __atomic_compare_exchange_n(p, &c, x, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return c; }
LONG64 InterlockedExchange64(volatile LONG64 *p, LONG64 v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
BOOL IsDebuggerPresent(void) { return FALSE; }
void DebugBreak(void) { raise(SIGTRAP); }
USHORT CaptureStackBackTrace(ULONG skip, ULONG count, void **frames, ULONG *hash) { (void)skip; (void)count; (void)frames; if (hash) *hash = 0; return 0; }
LONG CompareFileTime(const FILETIME *a, const FILETIME *b)
{
    uint64_t x = ((uint64_t)a->dwHighDateTime << 32) | a->dwLowDateTime, y = ((uint64_t)b->dwHighDateTime << 32) | b->dwLowDateTime;
    return x < y ? -1 : x > y ? 1 : 0;
}
BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value) { return (value ? setenv(name, value, 1) : unsetenv(name)) == 0; }
void PostQuitMessage(int code) { (void)code; }
BOOL DestroyWindow(HWND h) { (void)h; return TRUE; }
