#ifndef NV2A_IMMEDIATE_PACKET_H
#define NV2A_IMMEDIATE_PACKET_H
#include <stdint.h>
#include <stddef.h>
#include "nv2a_batch_probe.h"
/* Full, bounded validation of a following immediate strip. No GPU state is
 * touched and no words are consumed on failure. Acceptance is necessary, not
 * sufficient, for merging: texture feedback and translator side effects must
 * also be checked by the caller. */
typedef struct Nv2aImmediatePacket {
    uint32_t words, data_methods;
} Nv2aImmediatePacket;
static inline int nv2a_immediate_vertex_method(uint32_t m)
{
    return (m >= 0x1518u && m < 0x1528u) ||
           (m >= 0x1590u && m < 0x1598u) ||
           (m >= 0x1880u && m < 0x1b00u);
}
static inline int nv2a_parse_immediate_strip(const uint8_t *p, size_t available,
                                              uint32_t sub, Nv2aImmediatePacket *out)
{
    if (!p || !out || sub >= 8u || available < 16u) return 0;
    uint32_t be = (1u << 18) | (sub << 13) | 0x17fcu;
    if (nv2a_probe_le32(p) != be || nv2a_probe_le32(p + 4u) != 6u) return 0;
    size_t at = 8u;
    uint32_t methods = 0;
    /* Limit both work and speculative input to a small, bounded packet. */
    while (at <= available && available - at >= 4u && at / 4u < 512u) {
        uint32_t h = nv2a_probe_le32(p + at);
        if (h == be) {
            if (available - at < 8u || nv2a_probe_le32(p + at + 4u) != 0u || !methods) return 0;
            out->words = (uint32_t)(at / 4u + 2u);
            out->data_methods = methods;
            return 1;
        }
        uint32_t type = h & 0xe0030003u, n = (h >> 18) & 0x7ffu;
        uint32_t method = h & 0x1fffu;
        if ((type != 0u && type != 0x40000000u) || !n ||
            ((h >> 13) & 7u) != sub || n > (available - at - 4u) / 4u ||
            n > 512u - at / 4u - 1u) return 0;
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t m = method + (type == 0u ? i * 4u : 0u);
            if (!nv2a_immediate_vertex_method(m)) return 0;
        }
        methods += n;
        at += ((size_t)n + 1u) * 4u;
    }
    return 0;
}
#endif
