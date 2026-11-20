#ifndef NV2A_ENV_CACHE_H
#define NV2A_ENV_CACHE_H
#include <stdint.h>
#include <stdlib.h>
/* Android launch configuration is immutable once the renderer starts. Every
 * call site must use a string literal: cache its result (including NULL) at
 * that site so hot draws avoid repeated table hashing/probing. The atomics
 * permit concurrent first use; duplicate getenv calls are harmless. */
#define NV2A_LAUNCH_ENV(name) __extension__ ({ \
    (void)sizeof("" name); \
    static const char *nv2a_env_value = (const char *)(uintptr_t)1; \
    const char *nv2a_env_result = __atomic_load_n(&nv2a_env_value, __ATOMIC_ACQUIRE); \
    if (nv2a_env_result == (const char *)(uintptr_t)1) { \
        nv2a_env_result = getenv(name); \
        __atomic_store_n(&nv2a_env_value, nv2a_env_result, __ATOMIC_RELEASE); \
    } \
    nv2a_env_result; \
})
#endif
