#ifndef NV2A_FEEDBACK_REUSE_H
#define NV2A_FEEDBACK_REUSE_H

/* Only the immediately previous stage is eligible: no intervening surface
 * resolves or guest draw can have modified the bound render target. Each
 * snapshot is scoped to one draw and starts with all entries empty. */
static int nv2a_feedback_reuse_previous(unsigned stage,
                                         const void *previous_texture,
                                         const void *previous_view)
{
    return stage > 0u && previous_texture != 0 && previous_view != 0;
}
#endif
