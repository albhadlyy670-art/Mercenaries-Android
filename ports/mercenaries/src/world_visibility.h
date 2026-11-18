#ifndef MERCENARIES_WORLD_VISIBILITY_H
#define MERCENARIES_WORLD_VISIBILITY_H
#include <math.h>
/* Read-only conservative classification. This is NOT permission to suppress
 * rendering: bounds and pass identity must be verified for each object. The
 * retail RedScene query places its center along minus camera axis +0x30. */
typedef struct WorldVisibilitySphere { float x,y,z,r; } WorldVisibilitySphere;
static inline int world_sphere_behind(const float position[3], const float backward[3],
                                      WorldVisibilitySphere sphere)
{
    if (!position || !backward || !isfinite(sphere.x) || !isfinite(sphere.y) ||
        !isfinite(sphere.z) || !isfinite(sphere.r) || sphere.r < 0.0f) return -1;
    for (unsigned i=0; i<3; ++i)
        if (!isfinite(position[i]) || !isfinite(backward[i])) return -1;
    double x=(double)sphere.x-position[0], y=(double)sphere.y-position[1],
           z=(double)sphere.z-position[2];
    double len2=(double)backward[0]*backward[0]+(double)backward[1]*backward[1]+
                (double)backward[2]*backward[2];
    if (!isfinite(len2) || len2 < 0.25 || len2 > 4.0) return -1;
    double dot=x*backward[0]+y*backward[1]+z*backward[2];
    return dot > (double)sphere.r*sqrt(len2);
}
#endif
