#ifndef D3D11_GLES_DRAW_LIMIT_H
#define D3D11_GLES_DRAW_LIMIT_H
#include <stdint.h>
/* Fixed windows start with the first eligible draw, not aligned with the
 * independent performance logger. Single render-thread use only. */
typedef struct DrawLimitWindow {
    double start;
    uint32_t submitted;
    uint64_t skipped;
} DrawLimitWindow;
static inline int draw_limit_block(DrawLimitWindow *w, double now, uint32_t limit,
                                   DrawLimitWindow *completed)
{
    if (completed) *completed = (DrawLimitWindow){0};
    if (!limit || now <= 0.0) return 0; /* disabled or clock failure */
    if (w->start <= 0.0 || now < w->start || now - w->start >= 10.0) {
        if (completed && w->start > 0.0) *completed = *w;
        *w = (DrawLimitWindow){ .start = now };
    }
    if (w->submitted < limit) return 0;
    ++w->skipped;
    return 1;
}
static inline void draw_limit_submitted(DrawLimitWindow *w, double now, uint32_t limit)
{ if (limit && now > 0.0 && w->submitted < limit) ++w->submitted; }
#endif
