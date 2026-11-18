#include <assert.h>
#include <stdio.h>
#include "../../../src/nv2a/nv2a_feedback_reuse.h"
int main(void)
{
    int tex, view;
    assert(!nv2a_feedback_reuse_previous(0, &tex, &view));
    assert(!nv2a_feedback_reuse_previous(1, 0, &view));
    assert(!nv2a_feedback_reuse_previous(1, &tex, 0));
    assert(nv2a_feedback_reuse_previous(1, &tex, &view));
    assert(nv2a_feedback_reuse_previous(3, &tex, &view));
    puts("feedback reuse: only adjacent completed snapshots qualify");
    return 0;
}
