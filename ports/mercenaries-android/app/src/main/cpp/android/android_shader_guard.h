#pragma once
#include <stdint.h>

/* GL calls run on the game/render thread. These functions must be called on
 * that same thread; a pending marker is deliberately left on process death. */
uint64_t android_shader_pair_hash(const char *vertex, const char *fragment);
int android_shader_pair_bad(uint64_t hash);
void android_shader_guard_begin(uint64_t hash, const char *stage);
void android_shader_guard_end(void);
