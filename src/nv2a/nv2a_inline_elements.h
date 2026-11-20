#ifndef NV2A_INLINE_ELEMENTS_H
#define NV2A_INLINE_ELEMENTS_H
#include <stdint.h>
#include <stddef.h>
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

/* Each guest word contains two unsigned 16-bit indices, low half first.
 * The caller owns enough output space and has already bounded the count. */
static inline void nv2a_unpack_inline_elements16(uint32_t *dst,
                                                  const uint32_t *src,
                                                  uint32_t count)
{
    uint32_t i = 0;
#if defined(__aarch64__) && defined(__ARM_NEON)
    const uint32x4_t mask = vdupq_n_u32(0xffffu);
    for (; count - i >= 4u; i += 4u) {
        uint32x4_t words = vld1q_u32(src + i);
        uint32x4x2_t pair;
        pair.val[0] = vandq_u32(words, mask);
        pair.val[1] = vshrq_n_u32(words, 16);
        vst2q_u32(dst + (size_t)i * 2u, pair);
    }
#endif
    for (; i < count; ++i) {
        uint32_t word = src[i];
        dst[(size_t)i * 2u] = word & 0xffffu;
        dst[(size_t)i * 2u + 1u] = word >> 16;
    }
}
#endif
