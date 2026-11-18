"""Run the actual PGRAPH batching gate against fake translator state."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/nv2a/nv2a_pgraph_d3d11.c').read_text()
start=s.index('int pgraph_d3d11_try_batch_inline_triangles(')
pos=s.index('{',start); depth=1; end=pos+1
while depth:
    depth+=(s[end]=='{')-(s[end]=='}'); end+=1
fn=s[start:end]
code=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "nv2a_draw_batch.h"
#define NV097_SET_BEGIN_END_OP_TRIANGLES 5
static struct {
    int initialized, in_draw, inline_array_mode;
    uint32_t draw_mode, draw_array_count, inline_element_count, immediate_vertex_count;
    struct { int enabled; } tex[4];
    uint32_t inline_count, inline_data[512];
    struct { uint32_t methods_handled; } stats;
} g_pg;
static uint32_t stride=4;
static uint32_t prepare_inline_array_layout(void){return stride;}
static unsigned merges;
#define PGRAPH_BATCH_MERGED() (++merges)
''' + fn + r'''
int main(void) {
    uint32_t next[]={4,5,6};
    g_pg.initialized=g_pg.in_draw=g_pg.inline_array_mode=1; g_pg.draw_mode=5;
    g_pg.inline_count=3; g_pg.inline_data[0]=1;g_pg.inline_data[1]=2;g_pg.inline_data[2]=3;
    g_pg.tex[2].enabled=1; assert(!pgraph_d3d11_try_batch_inline_triangles(next,3));
    assert(g_pg.inline_count==3 && !merges);
    g_pg.tex[2].enabled=0; assert(!pgraph_d3d11_try_batch_inline_triangles(g_pg.inline_data,3));
    g_pg.draw_mode=6; assert(!pgraph_d3d11_try_batch_inline_triangles(next,3));
    g_pg.draw_mode=5; assert(!pgraph_d3d11_try_batch_inline_triangles(next,2));
    stride=8; assert(!pgraph_d3d11_try_batch_inline_triangles(next,3)); stride=4;
    assert(pgraph_d3d11_try_batch_inline_triangles(next,3));
    assert(g_pg.inline_count==6 && g_pg.stats.methods_handled==5 && merges==1);
    for(int i=0;i<6;i++) assert(g_pg.inline_data[i]==(unsigned)i+1);
    g_pg.inline_count=120; assert(!pgraph_d3d11_try_batch_inline_triangles(next,3));
    puts("live batch gate: eligible concatenation and unsafe cases rejected OK");
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'t.c';p.write_text(code); exe=Path(d)/'t'
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'src/nv2a'),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
