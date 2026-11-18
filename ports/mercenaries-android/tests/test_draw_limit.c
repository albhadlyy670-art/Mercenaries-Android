#include <assert.h>
#include <stdio.h>
#include "../app/src/main/cpp/d3d11_gles/draw_limit.h"
int main(void)
{
    DrawLimitWindow w={0}, done={0};
    assert(!draw_limit_block(&w,1,0,&done) && !w.start);
    for (unsigned i=0;i<25000;i++) {
        assert(!draw_limit_block(&w,1.0+i*0.0001,25000,&done));
        draw_limit_submitted(&w,1.0+i*0.0001,25000);
    }
    assert(draw_limit_block(&w,3.6,25000,&done));
    assert(draw_limit_block(&w,10.999,25000,&done));
    assert(w.submitted==25000 && w.skipped==2);
    assert(!draw_limit_block(&w,11.0,25000,&done));
    assert(done.submitted==25000 && done.skipped==2 && w.submitted==0);
    /* Failed prepare consumes no budget, and invalid clocks fail open. */
    assert(!draw_limit_block(&w,11.1,25000,&done) && w.submitted==0);
    assert(!draw_limit_block(&w,0,25000,&done) && w.submitted==0);
    draw_limit_submitted(&w,11.1,25000); assert(w.submitted==1);
    assert(!draw_limit_block(&w,5,25000,&done) && done.submitted==1 && w.submitted==0);
    puts("draw limit: 25000 calls, skips, rollover and fail-open OK");
}
