/*
 * mmio_a64.h - AArch64 load/store decoder for guest MMIO trapping.
 *
 * The Windows build traps accesses to unmapped guest MMIO pages (NV2A, APU,
 * AC'97) with a vectored exception handler that decodes the faulting *x86-64*
 * instruction. On Android the host CPU is AArch64, so the same job needs an
 * AArch64 decoder. Recompiled code reaches MMIO through
 * `*(volatile uintN_t *)ptr`, which the compiler lowers to a single
 * LDR/STR-class instruction, so only the single-register integer load/store
 * encodings need to be understood:
 *
 *   unsigned offset   sz 111 0 01 opc imm12 Rn Rt
 *   register offset   sz 111 0 00 opc 1 Rm opt S 10 Rn Rt
 *   unscaled (LDUR)   sz 111 0 00 opc 0 imm9 00 Rn Rt
 *   post-index        sz 111 0 00 opc 0 imm9 01 Rn Rt
 *   pre-index         sz 111 0 00 opc 0 imm9 11 Rn Rt
 *
 * Anything else (SIMD/FP, pairs, exclusives, atomics) is reported as
 * undecodable; the caller then treats the fault as a real crash.
 */
#ifndef MMIO_A64_H
#define MMIO_A64_H

#include <stdbool.h>
#include <stdint.h>

/* Register file captured from the signal's ucontext by the SIGSEGV handler and
 * handed to the NV2A/APU hooks as their opaque PCONTEXT. */
typedef struct {
    uint64_t x[31];
    uint64_t sp;
    uint64_t pc;
} mmio_a64_ctx_t;

typedef struct {
    bool     is_store;
    unsigned size;        /* access size in bytes: 1, 2, 4, 8 */
    unsigned rt;          /* 0..31 (31 = XZR for data) */
    unsigned rn;          /* base register 0..31 (31 = SP) */
    bool     sign_extend; /* LDRS* */
    bool     dest64;      /* destination register is Xt (else Wt, zero-extends) */
    bool     writeback;   /* pre/post-indexed: Rn is updated */
    int32_t  wb_delta;    /* value added to Rn when writeback */
} mmio_a64_t;

static inline bool mmio_a64_decode(uint32_t insn, mmio_a64_t *d)
{
    if (((insn >> 27) & 7u) != 7u) return false;      /* load/store reg class */
    if ((insn >> 26) & 1u) return false;              /* V=1: SIMD/FP */

    const unsigned size = (insn >> 30) & 3u;
    const unsigned opc  = (insn >> 22) & 3u;
    const unsigned rn   = (insn >> 5) & 31u;
    const unsigned rt   = insn & 31u;
    const unsigned b2524 = (insn >> 24) & 3u;

    bool writeback = false;
    int32_t delta = 0;

    if (b2524 == 1u) {
        /* unsigned scaled immediate: no writeback */
    } else if (b2524 == 0u) {
        if ((insn >> 21) & 1u) {
            if (((insn >> 10) & 3u) != 2u) return false;  /* atomics etc. */
            /* register offset */
        } else {
            const unsigned mode = (insn >> 10) & 3u;
            int32_t imm9 = (int32_t)((insn >> 12) & 0x1FFu);
            if (imm9 & 0x100) imm9 -= 0x200;
            if (mode == 0u) {
                /* LDUR/STUR: no writeback */
            } else if (mode == 1u || mode == 3u) {
                writeback = true;                         /* post / pre */
                delta = imm9;
            } else {
                return false;                             /* LDTR/STTR */
            }
        }
    } else {
        return false;
    }

    d->size = 1u << size;
    d->rt = rt;
    d->rn = rn;
    d->writeback = writeback;
    d->wb_delta = delta;
    d->sign_extend = false;
    d->dest64 = (size == 3u);

    if (opc == 0u) {
        d->is_store = true;
    } else if (opc == 1u) {
        d->is_store = false;
    } else {
        /* opc 10/11: sign-extending loads (PRFM when size==3 && opc==2) */
        if (size == 3u) return false;
        if (opc == 3u && size == 2u) return false;        /* no LDRSW to W */
        d->is_store = false;
        d->sign_extend = true;
        d->dest64 = (opc == 2u);
    }
    return true;
}

/* Value a store writes, truncated to the access size. x[31] is unused (XZR). */
static inline uint64_t mmio_a64_store_value(const uint64_t x[31], const mmio_a64_t *d)
{
    uint64_t v = (d->rt == 31u) ? 0u : x[d->rt];
    switch (d->size) {
    case 1:  return v & 0xFFu;
    case 2:  return v & 0xFFFFu;
    case 4:  return v & 0xFFFFFFFFu;
    default: return v;
    }
}

/* Complete the instruction after emulation. `load_value` is the raw value read
 * from the device (ignored for stores). Updates x[] and applies writeback. */
static inline void mmio_a64_finish(uint64_t x[31], uint64_t *sp,
                                   const mmio_a64_t *d, uint64_t load_value)
{
    if (!d->is_store && d->rt != 31u) {
        uint64_t v = load_value;
        switch (d->size) {
        case 1: v &= 0xFFu; if (d->sign_extend) v = (uint64_t)(int64_t)(int8_t)v;  break;
        case 2: v &= 0xFFFFu; if (d->sign_extend) v = (uint64_t)(int64_t)(int16_t)v; break;
        case 4: v &= 0xFFFFFFFFu; if (d->sign_extend) v = (uint64_t)(int64_t)(int32_t)v; break;
        default: break;
        }
        if (!d->dest64) v &= 0xFFFFFFFFu;                 /* Wt write zero-extends */
        x[d->rt] = v;
    }
    if (d->writeback) {
        if (d->rn == 31u) *sp += (int64_t)d->wb_delta;
        else x[d->rn] += (int64_t)d->wb_delta;
    }
}

#endif /* MMIO_A64_H */
