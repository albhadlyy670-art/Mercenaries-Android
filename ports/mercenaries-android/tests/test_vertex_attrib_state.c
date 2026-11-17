/* Mock GLES VAO state to verify transitions, defaults, and repeated draws. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef unsigned GLuint;
typedef float GLfloat;
static unsigned enabled, enables, disables, defaults;
static void glEnableVertexAttribArray(GLuint i) { enabled |= 1u << i; ++enables; }
static void glDisableVertexAttribArray(GLuint i) { enabled &= ~(1u << i); ++disables; }
static void glVertexAttrib4f(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{ assert(i < 16 && x == 0 && y == 0 && z == 0 && w == 1); ++defaults; }
#include "../app/src/main/cpp/d3d11_gles/vertex_attrib_state.h"
int main(void)
{
    uint32_t prev = 0; int initialized = 0;
    sync_vertex_attribs(5, &prev, &initialized);
    assert(enabled == 5 && enables == 2 && disables == 14 && defaults == 14);
    sync_vertex_attribs(5, &prev, &initialized);
    assert(enables == 2 && disables == 14 && defaults == 14);
    sync_vertex_attribs(6, &prev, &initialized);
    assert(enabled == 6 && enables == 3 && disables == 15 && defaults == 15);
    sync_vertex_attribs(0, &prev, &initialized);
    assert(enabled == 0 && enables == 3 && disables == 17 && defaults == 17);
    puts("vertex attributes: initial defaults, reuse and transitions OK"); return 0;
}
