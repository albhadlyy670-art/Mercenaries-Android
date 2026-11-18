#ifndef NV2A_PUSH_BATCH_H
#define NV2A_PUSH_BATCH_H
#include <stdint.h>
#include <stddef.h>
/* Only the exact END; BEGIN triangles; INLINE_ARRAY; END shape is accepted.
 * No jumps, other subchannels, state methods, or incomplete packets. */
static inline uint32_t nv2a_batch_inline_packet(const uint32_t *p, uint32_t available,
                                                uint32_t subchannel, uint32_t *payload_count)
{
    if (!p || !payload_count || subchannel >= 8u || available < 6u) return 0;
    const uint32_t begin_end = 0x17fcu | (subchannel << 13);
    const uint32_t inline_array = 0x1818u | (subchannel << 13);
    if (p[0] != ((1u << 18) | begin_end) || p[1] != 5u) return 0;
    uint32_t header = p[2], n = (header >> 18) & 0x7ffu;
    if (!n || header != (0x40000000u | (n << 18) | inline_array) || n > available - 5u) return 0;
    if (p[3u+n] != ((1u << 18) | begin_end) || p[4u+n] != 0u) return 0;
    *payload_count = n;
    return n + 5u;
}
#endif
