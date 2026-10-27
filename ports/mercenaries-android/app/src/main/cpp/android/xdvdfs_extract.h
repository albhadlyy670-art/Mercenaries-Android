/*
 * xdvdfs_extract.h - read-only XDVDFS (original Xbox disc) extractor.
 *
 * Reads the ISO through a plain file descriptor with pread(), so it works
 * with a descriptor handed over by Android's Storage Access Framework
 * (ContentResolver.openFileDescriptor -> detachFd) without copying the
 * multi-gigabyte image first.
 */
#ifndef XDVDFS_EXTRACT_H
#define XDVDFS_EXTRACT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return non-zero from the callback to cancel the extraction. */
typedef int (*xiso_progress_fn)(uint64_t bytes_done, uint64_t bytes_total,
                                const char *current_path, void *user);

enum {
    XISO_OK = 0,
    XISO_ERR_IO = -1,        /* read/write/mkdir failure */
    XISO_ERR_FORMAT = -2,    /* not an XDVDFS image */
    XISO_ERR_CANCELLED = -3,
    XISO_ERR_NOMEM = -4
};

/* Locate the XDVDFS volume descriptor. Tries the plain XISO layout and the
 * known redump/XGD partition offsets. Returns 0 and fills *partition_offset
 * (bytes), *root_sector and *root_size, or XISO_ERR_FORMAT. */
int xiso_probe(int fd, uint64_t *partition_offset, uint32_t *root_sector,
               uint32_t *root_size);

/* Total payload bytes and file count (for a progress bar / free-space check). */
int xiso_measure(int fd, uint64_t *total_bytes, uint32_t *file_count);

/* Extract the whole file system below dest_root (created if needed).
 * err (optional) receives a short human-readable message on failure. */
int xiso_extract(int fd, const char *dest_root, xiso_progress_fn progress,
                 void *user, char *err, size_t err_len);

#ifdef __cplusplus
}
#endif
#endif
