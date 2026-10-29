/*
 * android_fault.c - SIGSEGV dispatcher: the Android equivalent of the
 * Windows vectored exception handler in ports/mercenaries/src/main.c.
 *
 * Guest MMIO (NV2A 0xFD000000, APU 0xFE800000, AC'97 0xFEC00000) is simply
 * unmapped PROT_NONE address space inside the 4 GiB guest arena. A load or
 * store there raises SIGSEGV; we decode the AArch64 instruction, call the
 * device model, advance PC and resume. Faults outside the arena, or that no
 * device claims, fall through to Android's default crash handling (tombstone).
 */
#define _GNU_SOURCE
#include "platform/xbox_winnt.h"
#include "platform/mmio_a64.h"
#include "kernel/xbox_memory_layout.h"
#include "kernel/preview_log.h"
#include "nv2a/nv2a_mmio_hook.h"
#include "apu/apu_mmio_hook.h"

#include <android/log.h>
#include <stdint.h>
extern void mercenaries_android_crash_report(const char *what, uintptr_t fault, uintptr_t host_pc);
#include <signal.h>
#if defined(__aarch64__)
#include <asm/sigcontext.h>      /* struct esr_context, ESR_MAGIC */
#endif
#include <stdio.h>
#include <string.h>
#include <ucontext.h>

#define LOG(...) __android_log_print(ANDROID_LOG_ERROR, "MercRecomp", __VA_ARGS__)

extern ptrdiff_t g_xbox_mem_offset;
static struct sigaction g_previous;

/* ESR_EL1 data-abort write-not-read bit, from the kernel's esr_context record. */
static int fault_is_write(const ucontext_t *uc)
{
#if defined(__aarch64__)
    const uint8_t *p = (const uint8_t *)uc->uc_mcontext.__reserved;
    const uint8_t *end = p + sizeof(uc->uc_mcontext.__reserved);
    while (p + sizeof(struct _aarch64_ctx) <= end) {
        const struct _aarch64_ctx *h = (const struct _aarch64_ctx *)p;
        if (h->magic == 0 || h->size == 0) break;
        if (h->magic == ESR_MAGIC) {
            const struct esr_context *e = (const struct esr_context *)p;
            return (int)((e->esr >> 6) & 1u);          /* ISS.WnR */
        }
        p += h->size;
    }
#else
    (void)uc;
#endif
    return 0;
}

static void segv_handler(int sig, siginfo_t *si, void *ucv)
{
#if defined(__aarch64__)
    ucontext_t *uc = (ucontext_t *)ucv;
    const uintptr_t fault = (uintptr_t)si->si_addr;
    const uintptr_t base = (uintptr_t)g_xbox_mem_offset;

    if (si->si_signo == SIGSEGV && base && fault >= base && fault - base < 0x100000000ull) {
        const uint32_t va = (uint32_t)(fault - base);
        const int is_write = fault_is_write(uc);
        mmio_a64_ctx_t c;
        bool handled = false;

        for (int i = 0; i < 31; ++i) c.x[i] = uc->uc_mcontext.regs[i];
        c.sp = uc->uc_mcontext.sp;
        c.pc = uc->uc_mcontext.pc;

        if (va >= 0x80000000u && va < 0xC0000000u && xbox_MapPhysicalAlias(va))
            return;                                    /* retry the same instruction */
        if (va >= 0xFD000000u && va < 0xFE000000u)
            handled = nv2a_hook_handle_mmio((PCONTEXT)&c, fault, va, is_write);
        else if (va >= 0xFE800000u && va < 0xFE880000u)
            handled = apu_hook_handle_mmio((PCONTEXT)&c, fault, va, is_write);
        else if (va >= 0xFEC00000u && va < 0xFEC01000u)
            handled = ac97_hook_handle_mmio((PCONTEXT)&c, fault, va, is_write);
        else if (va >= 0xF0000000u && va < 0xFD000000u)
            handled = nv2a_hook_handle_vram(fault, va);
        else if (va >= 0x00010000u && va < 0x08000000u)
            handled = false;                           /* guest RAM must never fault */

        if (handled) {
            for (int i = 0; i < 31; ++i) uc->uc_mcontext.regs[i] = c.x[i];
            uc->uc_mcontext.sp = c.sp;
            uc->uc_mcontext.pc = c.pc;
            return;
        }
        LOG("unhandled guest fault: va=%08X host=%p pc=%p %s",
            va, (void *)fault, (void *)(uintptr_t)uc->uc_mcontext.pc, is_write ? "write" : "read");
        mercenaries_android_crash_report("unhandled guest memory fault", fault, (uintptr_t)uc->uc_mcontext.pc);
    }
#else
    (void)ucv; (void)si;
#endif
    /* Not ours: restore the previous disposition and re-execute so the system
     * crash reporter produces a normal tombstone. */
    sigaction(sig, &g_previous, NULL);
}

void android_install_fault_handler(void)
{
    /* Run on a dedicated stack so a guest stack overflow is still reportable. */
    static uint8_t altstack[64 * 1024];
    stack_t ss = { .ss_sp = altstack, .ss_size = sizeof(altstack), .ss_flags = 0 };
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &g_previous);
}
