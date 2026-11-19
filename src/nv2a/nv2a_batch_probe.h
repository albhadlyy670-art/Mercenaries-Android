#ifndef NV2A_BATCH_PROBE_H
#define NV2A_BATCH_PROBE_H
#include <stddef.h>
#include <stdint.h>
/* Observation only: read the next packet without consuming or modifying it.
 * A direct BEGIN is not proof of a safe merge: subsequent methods, resource
 * aliasing and renderer per-draw effects must still be examined. */
typedef enum Nv2aBatchProbe {
    NV2A_PROBE_BOUNDARY, NV2A_PROBE_BARRIER, NV2A_PROBE_BEGIN_OTHER,
    NV2A_PROBE_STRIP_PARTIAL, NV2A_PROBE_STRIP_INDEXED,
    NV2A_PROBE_STRIP_ARRAY, NV2A_PROBE_STRIP_INLINE,
    NV2A_PROBE_STRIP_IMMEDIATE, NV2A_PROBE_STRIP_OTHER
} Nv2aBatchProbe;
static inline uint32_t nv2a_probe_le32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static inline Nv2aBatchProbe nv2a_probe_next(const uint8_t *p, size_t available, uint32_t sub)
{
    if (!p || sub >= 8 || available < 8) return NV2A_PROBE_BOUNDARY;
    if (nv2a_probe_le32(p) != ((1u << 18) | (sub << 13) | 0x17fcu))
        return NV2A_PROBE_BARRIER;
    if (nv2a_probe_le32(p + 4) != 6u) return NV2A_PROBE_BEGIN_OTHER;
    if (available < 12) return NV2A_PROBE_STRIP_PARTIAL;
    uint32_t h = nv2a_probe_le32(p + 8), method = h & 0x1fffu;
    if (((h & 0xe0030003u) != 0u && (h & 0xe0030003u) != 0x40000000u) ||
        ((h >> 13) & 7u) != sub || !((h >> 18) & 0x7ffu)) return NV2A_PROBE_STRIP_OTHER;
    if (method == 0x1800u || method == 0x1808u) return NV2A_PROBE_STRIP_INDEXED;
    if (method == 0x1810u) return NV2A_PROBE_STRIP_ARRAY;
    if (method == 0x1818u) return NV2A_PROBE_STRIP_INLINE;
    if ((method >= 0x1518u && method < 0x1528u) ||
        (method >= 0x1590u && method < 0x1598u) ||
        (method >= 0x1880u && method < 0x1900u) ||
        (method >= 0x1900u && method < 0x1940u) ||
        (method >= 0x1940u && method < 0x1980u) ||
        (method >= 0x1980u && method < 0x1a00u) ||
        (method >= 0x1a00u && method < 0x1b00u)) return NV2A_PROBE_STRIP_IMMEDIATE;
    return NV2A_PROBE_STRIP_OTHER;
}
#endif
