#ifndef D3D8_VERTEX_RING_CACHE_H
#define D3D8_VERTEX_RING_CACHE_H
#include <stdint.h>
#include <string.h>
/* Small exact-byte reuse cache, valid only until the ring is discarded.
 * It reuses storage, not draw calls; all draws still execute in order. */
#define D3D8_VERTEX_CACHE_ENTRIES 4u
#define D3D8_VERTEX_CACHE_MAX_BYTES 128u
typedef struct D3D8VertexCacheEntry {
    uint32_t offset, size;
    uint8_t bytes[D3D8_VERTEX_CACHE_MAX_BYTES];
} D3D8VertexCacheEntry;
typedef struct D3D8VertexRingCache {
    D3D8VertexCacheEntry entry[D3D8_VERTEX_CACHE_ENTRIES];
    unsigned next;
} D3D8VertexRingCache;
static inline void d3d8_vertex_cache_clear(D3D8VertexRingCache *c)
{ memset(c, 0, sizeof(*c)); }
static inline int d3d8_vertex_cache_find(const D3D8VertexRingCache *c,
    const void *data, uint32_t size, uint32_t *offset)
{
    if (!data || !offset || !size || size > D3D8_VERTEX_CACHE_MAX_BYTES) return 0;
    for (unsigned i = 0; i < D3D8_VERTEX_CACHE_ENTRIES; ++i) {
        const D3D8VertexCacheEntry *e = &c->entry[i];
        if (e->size == size && !memcmp(e->bytes, data, size)) {
            *offset = e->offset; return 1;
        }
    }
    return 0;
}
static inline void d3d8_vertex_cache_store(D3D8VertexRingCache *c,
    const void *data, uint32_t size, uint32_t offset)
{
    if (!data || !size || size > D3D8_VERTEX_CACHE_MAX_BYTES) return;
    D3D8VertexCacheEntry *e = &c->entry[c->next++ % D3D8_VERTEX_CACHE_ENTRIES];
    e->offset = offset; e->size = size; memcpy(e->bytes, data, size);
}
#endif
