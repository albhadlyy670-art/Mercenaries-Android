/* Upload a complete, initialized constant-buffer snapshot only when changed.
 * The caller owns the cache and invalidates it when the D3D buffer is replaced. */
#ifndef D3D8_CONSTANT_CACHE_H
#define D3D8_CONSTANT_CACHE_H
#include <string.h>

static BOOL d3d8_upload_cached_constants(ID3D11DeviceContext *ctx,
    ID3D11Buffer *buffer, const void *value, void *previous, size_t size,
    BOOL *valid)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr;
    if (!buffer) return FALSE;
    if (*valid && memcmp(value, previous, size) == 0) return TRUE;
    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)buffer, 0,
                                D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, value, size);
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)buffer, 0);
        memcpy(previous, value, size);
        *valid = TRUE;
        return TRUE;
    }
    return FALSE;
}
#endif
