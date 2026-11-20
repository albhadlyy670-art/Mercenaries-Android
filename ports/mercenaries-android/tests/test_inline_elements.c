#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/nv2a/nv2a_inline_elements.h"

int main(void)
{
    uint32_t words[67], got[136], expected[136];
    uint32_t seed = 0x12345678u;
    for (unsigned n = 0; n <= 67; ++n) {
        for (unsigned i = 0; i < 67; ++i) {
            seed = seed * 1664525u + 1013904223u;
            words[i] = seed;
        }
        words[0] = 0xffff0000u;
        words[1] = 0x0000ffffu;
        memset(got, 0xa5, sizeof(got));
        memset(expected, 0xa5, sizeof(expected));
        for (unsigned i = 0; i < n; ++i) {
            expected[1 + 2*i] = words[i] & 0xffffu;
            expected[2 + 2*i] = words[i] >> 16;
        }
        nv2a_unpack_inline_elements16(got + 1, words, n);
        assert(!memcmp(got, expected, sizeof(got)));
    }
    puts("inline 16-bit elements: exact ordering, tails and bounds OK");
    return 0;
}
