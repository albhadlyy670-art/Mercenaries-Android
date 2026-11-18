/* Mock GLES VAO state to verify transitions, defaults, and repeated draws. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef unsigned GLuint;
typedef float GLfloat;
static unsigned enabled, enables, disables, defaults, divisor_calls;
static void glEnableVertexAttribArray(GLuint i) { enabled |= 1u << i; ++enables; }
static void glDisableVertexAttribArray(GLuint i) { enabled &= ~(1u << i); ++disables; }
static void glVertexAttrib4f(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{ assert(i < 16 && x == 0 && y == 0 && z == 0 && w == 1); ++defaults; }
static void glVertexAttribDivisor(GLuint i, GLuint d) { (void)i; (void)d; ++divisor_calls; }
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
    uint32_t valid = 0; GLuint divisors[16] = {0};
    sync_vertex_divisor(2, 0, &valid, divisors); sync_vertex_divisor(2, 0, &valid, divisors);
    assert(divisor_calls == 1);
    sync_vertex_divisor(2, 3, &valid, divisors); sync_vertex_divisor(3, 0, &valid, divisors);
    assert(divisor_calls == 3 && divisors[2] == 3 && divisors[3] == 0);
    puts("vertex attributes: defaults, divisors, reuse and transitions OK"); return 0;
}
