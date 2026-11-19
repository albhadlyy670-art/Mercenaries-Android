#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../../mercenaries/src/world_visibility.h"
int main(void)
{
    float pos[3]={0,0,0}, back[3]={0,0,1};
    assert(world_sphere_behind(pos,back,(WorldVisibilitySphere){0,0,10,1})==1);
    assert(world_sphere_behind(pos,back,(WorldVisibilitySphere){0,0,-10,1})==0);
    assert(world_sphere_behind(pos,back,(WorldVisibilitySphere){0,0,1,1})==0);
    assert(world_sphere_behind(pos,back,(WorldVisibilitySphere){0,0,2,3})==0);
    assert(world_sphere_behind(pos,back,(WorldVisibilitySphere){0,0,NAN,1})==-1);
    assert(world_sphere_behind(pos,back,(WorldVisibilitySphere){0,0,3,-1})==-1);
    back[2]=0; assert(world_sphere_behind(pos,back,(WorldVisibilitySphere){0,0,3,1})==-1);
    back[2]=2; assert(world_sphere_behind(pos,back,(WorldVisibilitySphere){0,0,3,1})==1);
    puts("world sphere visibility: behind, overlap and invalid bounds OK");
}
