#include <assert.h>
#include <stdio.h>
#include "../../../src/nv2a/nv2a_draw_workload.h"
int main(void)
{
    Nv2aDrawWorkload w = {0};
    nv2a_workload_draw(&w, NV2A_DRAW_ARRAY, 6, 4, 1);
    nv2a_workload_draw(&w, NV2A_DRAW_INDEXED, 6, 16, 0);
    nv2a_workload_draw(&w, NV2A_DRAW_INLINE, 6, 3, 1);
    nv2a_workload_draw(&w, NV2A_DRAW_IMMEDIATE, 6, 8, 0);
    nv2a_workload_draw(&w, NV2A_DRAW_ARRAY, 5, 3, 1);
    nv2a_workload_draw(&w, NV2A_DRAW_ARRAY, 6, 17, 1);
    assert(w.array == 3 && w.indexed == 1 && w.inline_array == 1 && w.immediate == 1);
    assert(w.small_strips == 4 && w.small_strips_array == 1 && w.small_strips_indexed == 1);
    assert(w.small_strips_inline == 1 && w.small_strips_immediate == 1 && w.small_strips_textured == 2);
    nv2a_workload_fog(&w,1,1,0x2601,2);
    nv2a_workload_fog(&w,1,0,0x800,1);
    nv2a_workload_fog(&w,0,1,0,0);
    assert(w.fog_enabled==2 && w.fog_disabled==1 && w.fog_programmable==1);
    assert(w.last_fog_mode==0x800 && w.last_fog_gen_mode==1);
    puts("NV2A workload: draw sources, sizes, topology and texture stage OK");
    return 0;
}
