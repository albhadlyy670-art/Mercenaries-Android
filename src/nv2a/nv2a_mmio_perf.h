#ifndef NV2A_MMIO_PERF_H
#define NV2A_MMIO_PERF_H
#include <stdint.h>

/* Accessed by the SIGSEGV handler and presentation thread. On Android AArch64
 * these operations must never call a locking libatomic fallback in a signal
 * handler. Counters wrap modulo 2^64; snapshots are individually consistent,
 * not a single atomic transaction across all three fields. */
#if defined(__ANDROID__) && defined(__aarch64__)
_Static_assert(__atomic_always_lock_free(sizeof(uint64_t), 0), "MMIO counters require lock-free 64-bit atomics");
#endif
typedef struct Nv2aMmioPerfCounts { uint64_t reads, writes, puts; } Nv2aMmioPerfCounts;
static inline void nv2a_mmio_perf_increment(uint64_t *v)
{ __atomic_fetch_add(v, 1, __ATOMIC_RELAXED); }
static inline Nv2aMmioPerfCounts nv2a_mmio_perf_snapshot(const Nv2aMmioPerfCounts *c)
{
    Nv2aMmioPerfCounts s = { __atomic_load_n(&c->reads, __ATOMIC_RELAXED),
        __atomic_load_n(&c->writes, __ATOMIC_RELAXED), __atomic_load_n(&c->puts, __ATOMIC_RELAXED) };
    return s;
}
static inline Nv2aMmioPerfCounts nv2a_mmio_perf_delta(Nv2aMmioPerfCounts now, Nv2aMmioPerfCounts before)
{ Nv2aMmioPerfCounts d = { now.reads - before.reads, now.writes - before.writes, now.puts - before.puts }; return d; }
/* Counts successfully handled fault-driven NV2A accesses. PUT is a subset of
 * writes; direct non-fault accesses and failed decodes are not counted. */
Nv2aMmioPerfCounts nv2a_mmio_get_perf_counts(void);
#endif
