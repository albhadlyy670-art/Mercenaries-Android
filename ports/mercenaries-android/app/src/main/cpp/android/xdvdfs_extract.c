/*
 * xdvdfs_extract.c - see xdvdfs_extract.h
 *
 * On-disc layout (2048-byte sectors, relative to the game partition):
 *   sector 32  volume descriptor
 *       +0x00  "MICROSOFT*XBOX*MEDIA"
 *       +0x14  u32 root directory sector
 *       +0x18  u32 root directory size
 *       +0x7EC "MICROSOFT*XBOX*MEDIA"
 *   directory = binary search tree of entries, offsets in 4-byte units:
 *       u16 left, u16 right, u32 start sector, u32 size, u8 attr, u8 namelen,
 *       name[namelen]; padded to 4 bytes. 0xFFFF child = none.
 */
#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE
#include "xdvdfs_extract.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SECTOR 2048u
#define MAGIC "MICROSOFT*XBOX*MEDIA"
#define MAGIC_LEN 20
#define ATTR_DIR 0x10u
#define MAX_DEPTH 32
#define MAX_DIR_BYTES (64u * 1024u * 1024u)
#define COPY_BUF (1u << 20)

static const uint64_t k_partition_offsets[] = {
    0ull,            /* plain XISO */
    0x18300000ull,   /* XGD1 redump (original Xbox) */
    0x0FD90000ull,   /* XGD2 */
    0x02080000ull,   /* XGD3 */
};

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int pread_full(int fd, void *buf, size_t len, uint64_t off)
{
    uint8_t *p = (uint8_t *)buf;
    while (len) {
        ssize_t n = pread(fd, p, len, (off_t)off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1; /* short image */
        p += n; off += (uint64_t)n; len -= (size_t)n;
    }
    return 0;
}

static void set_err(char *err, size_t n, const char *fmt, const char *arg)
{
    if (err && n) snprintf(err, n, fmt, arg ? arg : "");
}

int xiso_probe(int fd, uint64_t *partition_offset, uint32_t *root_sector,
               uint32_t *root_size)
{
    uint8_t vd[SECTOR];
    for (size_t i = 0; i < sizeof(k_partition_offsets) / sizeof(k_partition_offsets[0]); ++i) {
        uint64_t base = k_partition_offsets[i];
        if (pread_full(fd, vd, sizeof(vd), base + 32ull * SECTOR) != 0) continue;
        if (memcmp(vd, MAGIC, MAGIC_LEN) != 0) continue;
        if (memcmp(vd + 0x7EC, MAGIC, MAGIC_LEN) != 0) continue;
        uint32_t rs = rd32(vd + 0x14), sz = rd32(vd + 0x18);
        if (sz == 0 || sz > MAX_DIR_BYTES) continue;
        if (partition_offset) *partition_offset = base;
        if (root_sector) *root_sector = rs;
        if (root_size) *root_size = sz;
        return XISO_OK;
    }
    return XISO_ERR_FORMAT;
}

/* ---- directory walking --------------------------------------------------- */

typedef struct {
    int fd;
    uint64_t part;
    uint64_t total;       /* bytes seen/extracted target */
    uint64_t done;
    uint32_t files;
    int extracting;
    xiso_progress_fn progress;
    void *user;
    char *err;
    size_t err_len;
    uint8_t *buf;
} ctx_t;

typedef struct { char *name; uint32_t sector, size; uint8_t attr; } entry_t;

static int cmp_dummy(const void *a, const void *b) { (void)a; (void)b; return 0; }

/* Make a single path component safe to create on a POSIX file system. The Xbox file system is
 * case-insensitive while Android's is not, so every name is written in lower case; kernel_path.c folds
 * the game's requests the same way (src/kernel/kernel_path.c, XBOX_ANDROID). */
static int sanitize(char *name)
{
    if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..")) return -1;
    for (char *p = name; *p; ++p) {
        if (*p == '/' || *p == '\\' || *p == ':' || (unsigned char)*p < 0x20) *p = '_';
        else if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    }
    return 0;
}

static int mkdir_p(const char *path)
{
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return -1;
        *p = '/';
    }
    if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int copy_file(ctx_t *c, const char *dest, uint32_t sector, uint32_t size,
                     const char *shown)
{
    uint64_t src = c->part + (uint64_t)sector * SECTOR;
    int out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (out < 0) return XISO_ERR_IO;
    uint32_t left = size;
    while (left) {
        uint32_t chunk = left < COPY_BUF ? left : COPY_BUF;
        if (pread_full(c->fd, c->buf, chunk, src) != 0) { close(out); return XISO_ERR_IO; }
        const uint8_t *p = c->buf;
        uint32_t w = chunk;
        while (w) {
            ssize_t n = write(out, p, w);
            if (n < 0) {
                if (errno == EINTR) continue;
                close(out);
                return XISO_ERR_IO;
            }
            p += n; w -= (uint32_t)n;
        }
        src += chunk; left -= chunk; c->done += chunk;
        if (c->progress && c->progress(c->done, c->total, shown, c->user)) {
            close(out);
            return XISO_ERR_CANCELLED;
        }
    }
    if (close(out) != 0) return XISO_ERR_IO;
    return XISO_OK;
}

static int walk_dir(ctx_t *c, uint32_t sector, uint32_t size, const char *dest,
                    const char *rel, int depth)
{
    if (depth > MAX_DEPTH) return XISO_ERR_FORMAT;
    if (size == 0) return XISO_OK;
    if (size > MAX_DIR_BYTES) return XISO_ERR_FORMAT;

    uint8_t *dir = (uint8_t *)malloc(size);
    if (!dir) return XISO_ERR_NOMEM;
    if (pread_full(c->fd, dir, size, c->part + (uint64_t)sector * SECTOR) != 0) {
        free(dir);
        set_err(c->err, c->err_len, "Could not read a directory from the disc image%s", "");
        return XISO_ERR_IO;
    }

    /* Iterative in-order traversal with a visited bitmap (guards against
     * loops in a corrupt image). */
    uint32_t slots = size / 4u;
    uint8_t *seen = (uint8_t *)calloc((slots + 7u) / 8u, 1);
    uint32_t cap = 256, sp = 0;
    uint32_t *stack = (uint32_t *)malloc(cap * sizeof(uint32_t));
    int rc = XISO_OK;
    if (!seen || !stack) { rc = XISO_ERR_NOMEM; goto done; }

    uint32_t cur = 0;           /* offset in 4-byte units */
    int have_cur = 1;
    for (;;) {
        while (have_cur) {
            uint32_t byte_off = cur * 4u;
            if (cur >= slots || byte_off + 14u > size || (seen[cur >> 3] & (1u << (cur & 7)))) {
                have_cur = 0; break;
            }
            if (rd32(dir + byte_off) == 0xFFFFFFFFu) { have_cur = 0; break; } /* padding */
            seen[cur >> 3] |= (uint8_t)(1u << (cur & 7));
            if (sp == cap) {
                cap *= 2;
                uint32_t *ns = (uint32_t *)realloc(stack, cap * sizeof(uint32_t));
                if (!ns) { rc = XISO_ERR_NOMEM; goto done; }
                stack = ns;
            }
            stack[sp++] = cur;
            uint16_t left = rd16(dir + byte_off);
            if (left == 0xFFFFu) have_cur = 0; else cur = left;
        }
        if (sp == 0) break;
        uint32_t node = stack[--sp];
        const uint8_t *e = dir + (size_t)node * 4u;
        uint16_t right = rd16(e + 2);
        uint32_t esec = rd32(e + 4), esz = rd32(e + 8);
        uint8_t attr = e[12], nlen = e[13];
        if ((size_t)node * 4u + 14u + nlen > size || nlen == 0) { rc = XISO_ERR_FORMAT; goto done; }

        char name[256];
        memcpy(name, e + 14, nlen);
        name[nlen] = 0;
        if (sanitize(name) != 0) { rc = XISO_ERR_FORMAT; goto done; }

        char path[PATH_MAX], shown[PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", dest, name) >= (int)sizeof(path) ||
            snprintf(shown, sizeof(shown), "%s%s", rel, name) >= (int)sizeof(shown)) {
            rc = XISO_ERR_FORMAT; goto done;
        }

        if (attr & ATTR_DIR) {
            char sub_rel[PATH_MAX];
            snprintf(sub_rel, sizeof(sub_rel), "%s/", shown);
            if (c->extracting && mkdir_p(path) != 0) {
                set_err(c->err, c->err_len, "Could not create folder %s", path);
                rc = XISO_ERR_IO; goto done;
            }
            rc = walk_dir(c, esec, esz, path, sub_rel, depth + 1);
            if (rc != XISO_OK) goto done;
        } else {
            if (c->extracting) {
                rc = copy_file(c, path, esec, esz, shown);
                if (rc != XISO_OK) {
                    if (rc == XISO_ERR_IO)
                        set_err(c->err, c->err_len, "Could not write %s (storage full?)", path);
                    goto done;
                }
            } else {
                c->total += esz;
            }
            c->files++;
        }
        if (right == 0xFFFFu) { have_cur = 0; } else { cur = right; have_cur = 1; }
    }
done:
    free(stack); free(seen); free(dir);
    (void)cmp_dummy;
    return rc;
}

int xiso_measure(int fd, uint64_t *total_bytes, uint32_t *file_count)
{
    ctx_t c; memset(&c, 0, sizeof(c));
    uint32_t rs, rsz;
    c.fd = fd;
    int rc = xiso_probe(fd, &c.part, &rs, &rsz);
    if (rc != XISO_OK) return rc;
    rc = walk_dir(&c, rs, rsz, "", "", 0);
    if (rc != XISO_OK) return rc;
    if (total_bytes) *total_bytes = c.total;
    if (file_count) *file_count = c.files;
    return XISO_OK;
}

int xiso_extract(int fd, const char *dest_root, xiso_progress_fn progress,
                 void *user, char *err, size_t err_len)
{
    ctx_t c; memset(&c, 0, sizeof(c));
    uint32_t rs, rsz;
    c.fd = fd; c.progress = progress; c.user = user; c.err = err; c.err_len = err_len;
    if (err && err_len) err[0] = 0;

    int rc = xiso_probe(fd, &c.part, &rs, &rsz);
    if (rc != XISO_OK) {
        set_err(err, err_len, "This is not an original Xbox disc image%s", "");
        return rc;
    }
    /* Pass 1: size. */
    c.extracting = 0;
    rc = walk_dir(&c, rs, rsz, "", "", 0);
    if (rc != XISO_OK) {
        if (err && !err[0]) set_err(err, err_len, "The disc image is damaged%s", "");
        return rc;
    }
    c.done = 0; c.files = 0;
    if (mkdir_p(dest_root) != 0) {
        set_err(err, err_len, "Could not create %s", dest_root);
        return XISO_ERR_IO;
    }
    c.buf = (uint8_t *)malloc(COPY_BUF);
    if (!c.buf) return XISO_ERR_NOMEM;
    c.extracting = 1;
    rc = walk_dir(&c, rs, rsz, dest_root, "", 0);
    free(c.buf);
    if (rc == XISO_ERR_FORMAT && err && !err[0])
        set_err(err, err_len, "The disc image is damaged%s", "");
    return rc;
}
