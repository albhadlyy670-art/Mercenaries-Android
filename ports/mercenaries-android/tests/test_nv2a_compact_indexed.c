#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../../../src/nv2a/nv2a_indexed_vertices.h"
int main(void)
{
    uint8_t guest[6][32];
    uint32_t indices[24]; uint8_t expanded[24][32];
    for (unsigned i = 0; i < 6; ++i) memset(guest[i], (int)i + 10, 32);
    for (unsigned i = 0; i < 24; ++i) { indices[i] = 1000 + i % 6; memcpy(expanded[i], guest[i % 6], 32); }
    uint8_t scratch[512]; uint32_t ix[24]; Nv2aCompactIndexed out;
    assert(nv2a_compact_indexed_vertices(&expanded[0][0], indices, 24, 32, scratch, sizeof(scratch), ix, 24, &out));
    assert(out.vertices == scratch && out.indices == ix && out.vertex_count == 6);
    for (unsigned i = 0; i < 24; ++i)
        assert(!memcmp(out.vertices + out.indices[i] * 32, expanded[i], 32));
    nv2a_free_compact_indexed(&out, scratch, ix);
    assert(nv2a_compact_indexed_vertices(&expanded[0][0], indices, 24, 32, scratch, 8, ix, 2, &out));
    assert(out.vertices != scratch && out.indices != ix);
    for (unsigned i = 0; i < 24; ++i)
        assert(!memcmp(out.vertices + out.indices[i] * 32, expanded[i], 32));
    nv2a_free_compact_indexed(&out, scratch, ix);
    expanded[0][0] ^= 1;
    assert(!nv2a_compact_indexed_vertices(&expanded[0][0], indices, 24, 32, scratch, 8, ix, 2, &out));
    expanded[0][0] ^= 1;
    indices[3] = UINT32_MAX;
    assert(!nv2a_compact_indexed_vertices(&expanded[0][0], indices, 24, 32, scratch, sizeof(scratch), ix, 24, &out));
    for (unsigned i = 0; i < 24; ++i) indices[i] = i;
    assert(!nv2a_compact_indexed_vertices(&expanded[0][0], indices, 24, 32, scratch, sizeof(scratch), ix, 24, &out));
    assert(!nv2a_compact_indexed_vertices(&expanded[0][0], indices, 3, 32, scratch, sizeof(scratch), ix, 24, &out));
    puts("NV2A compact indexed: byte-for-byte sequence, sparse fallback and allocation OK");
}
