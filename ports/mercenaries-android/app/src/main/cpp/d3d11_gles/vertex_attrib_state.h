/* The renderer owns its VAO and is the only writer of generic attribute
 * defaults. Keep enabled arrays and disabled defaults in sync without issuing
 * the same 16 disable/default calls on every draw. */
#ifndef D3D11_GLES_VERTEX_ATTRIB_STATE_H
#define D3D11_GLES_VERTEX_ATTRIB_STATE_H
static void sync_vertex_attribs(uint32_t used, uint32_t *previous, int *initialized)
{
    uint32_t changed = *initialized ? (*previous ^ used) : 0xffffu;
    for (GLuint i = 0; i < 16; ++i) {
        uint32_t bit = 1u << i;
        if (!(changed & bit)) continue;
        if (used & bit) glEnableVertexAttribArray(i);
        else { glDisableVertexAttribArray(i); glVertexAttrib4f(i, 0.f, 0.f, 0.f, 1.f); }
    }
    *previous = used;
    *initialized = 1;
}
#endif
