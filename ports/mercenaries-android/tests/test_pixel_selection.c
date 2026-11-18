#include <assert.h>
#include <stdio.h>
static int combiner_available, combiner_calls, fixed_calls, selected;
static int d3d8_combiners_prepare_draw(void)
{ ++combiner_calls; if (combiner_available) selected = 1; return combiner_available; }
static void d3d8_shaders_prepare_pixel(void) { ++fixed_calls; selected = 2; }
#include "../../../src/d3d/d3d8_pixel_selection.h"
int main(void)
{
    combiner_available = 1; d3d8_prepare_selected_pixel();
    assert(selected == 1 && combiner_calls == 1 && fixed_calls == 0);
    combiner_available = 0; d3d8_prepare_selected_pixel();
    assert(selected == 2 && combiner_calls == 2 && fixed_calls == 1);
    combiner_available = 1; d3d8_prepare_selected_pixel();
    assert(selected == 1 && combiner_calls == 3 && fixed_calls == 1);
    puts("pixel pipeline: combiner and fixed-function fallback OK"); return 0;
}
