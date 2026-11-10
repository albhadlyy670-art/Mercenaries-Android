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
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#define LOG(...) __android_log_print(ANDROID_LOG_ERROR, "MercRecomp", __VA_ARGS__)

extern ptrdiff_t g_xbox_mem_offset;
extern volatile uint32_t g_recomp_current_func;
const char *g_android_crash_path;              /* set by android_main: <cache>/crash.log */
static volatile int g_in_crash;
static struct sigaction g_prev_other[32];

static void cw(int fd, const char *s) { if (fd >= 0) { ssize_t r = write(fd, s, strlen(s)); (void)r; } }
static void cw_addr(int fd, const char *label, uintptr_t a)
{
    char b[400]; Dl_info d;
    if (dladdr((void *)a, &d) && d.dli_fname) {
        const char *base = strrchr(d.dli_fname, '/'); base = base ? base + 1 : d.dli_fname;
        snprintf(b, sizeof(b), "%s %p  %s+0x%lx  %s\n", label, (void *)a, base,
                 (unsigned long)(a - (uintptr_t)d.dli_fbase), d.dli_sname ? d.dli_sname : "?");
    } else snprintf(b, sizeof(b), "%s %p\n", label, (void *)a);
    cw(fd, b);
}

/* Writes <cache>/crash.log directly (the stdout pump can lose its last lines when the process dies). */
static void dump_state(const char *tag, int flags, int sig, siginfo_t *si, void *ucv)
{
    int fd = g_android_crash_path ? open(g_android_crash_path, flags, 0644) : -1;
    char b[256];
    snprintf(b, sizeof(b), "%s signal %d code %d fault_addr %p guest_func %08X\n",
             tag, sig, si ? si->si_code : 0, si ? si->si_addr : NULL, (unsigned)g_recomp_current_func);
    cw(fd, b);
#if defined(__aarch64__)
    ucontext_t *uc = (ucontext_t *)ucv;
    uintptr_t pc = uc->uc_mcontext.pc, lr = uc->uc_mcontext.regs[30], fp = uc->uc_mcontext.regs[29];
    cw_addr(fd, "pc ", pc);
    cw_addr(fd, "lr ", lr);
    for (int i = 0; i < 24 && fp && !(fp & 7); ++i) {          /* frame-pointer walk */
        uintptr_t *f = (uintptr_t *)fp; uintptr_t ret = f[1], next = f[0];
        if (!ret) break;
        cw_addr(fd, "bt ", ret);
        if (next <= fp || next - fp > 0x100000) break;
        fp = next;
    }
#else
    (void)ucv;
#endif
    if (fd >= 0) close(fd);
}

static void report_crash(int sig, siginfo_t *si, void *ucv)
{
    if (g_in_crash) return;
    g_in_crash = 1;
    dump_state("[CRASH]", O_WRONLY | O_CREAT | O_TRUNC, sig, si, ucv);
}

/* Watchdog support: SIGUSR2 sent to the game thread appends one compact line (current guest function plus
 * a backtrace of function names) to crash.log. */
static void put_frame(char *buf, size_t cap, size_t *len, uintptr_t a)
{
    Dl_info d; char t[120];
    if (dladdr((void *)a, &d) && d.dli_fname) {
        const char *base = strrchr(d.dli_fname, '/'); base = base ? base + 1 : d.dli_fname;
        if (d.dli_sname) snprintf(t, sizeof(t), " %s", d.dli_sname);
        else snprintf(t, sizeof(t), " %s+%lx", base, (unsigned long)(a - (uintptr_t)d.dli_fbase));
    } else snprintf(t, sizeof(t), " %lx", (unsigned long)a);
    size_t n = strlen(t);
    if (*len + n + 2 < cap) { memcpy(buf + *len, t, n); *len += n; buf[*len] = 0; }
}

volatile int g_sample_label;   /* 0 = main game thread, otherwise the Xbox thread id being sampled */

static void hang_handler(int sig, siginfo_t *si, void *ucv)
{
    static int count;
    (void)sig; (void)si;
    int fd = g_android_crash_path ? open(g_android_crash_path, O_WRONLY | O_CREAT | O_APPEND, 0644) : -1;
    if (fd < 0) return;
    char line[900]; size_t len = 0;
    len = (size_t)snprintf(line, sizeof(line), "[S%d T%d] func=%08X:", ++count, g_sample_label, (unsigned)g_recomp_current_func);
#if defined(__aarch64__)
    ucontext_t *uc = (ucontext_t *)ucv;
    put_frame(line, sizeof(line), &len, uc->uc_mcontext.pc);
    put_frame(line, sizeof(line), &len, uc->uc_mcontext.regs[30]);
    uintptr_t fp = uc->uc_mcontext.regs[29];
    for (int i = 0; i < 12 && fp && !(fp & 7); ++i) {
        uintptr_t *f = (uintptr_t *)fp; uintptr_t ret = f[1], next = f[0];
        if (!ret) break;
        put_frame(line, sizeof(line), &len, ret);
        if (next <= fp || next - fp > 0x100000) break;
        fp = next;
    }
#else
    (void)ucv;
#endif
    line[len++] = '\n';
    ssize_t r = write(fd, line, len); (void)r;
    close(fd);
}
void android_install_hang_sampler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = hang_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR2, &sa, NULL);
}
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
        report_crash(sig, si, ucv);
    }
#else
    (void)ucv; (void)si;
#endif
    report_crash(sig, si, ucv);
    /* Not ours: restore the previous disposition and re-execute so the system
     * crash reporter produces a normal tombstone. */
    sigaction(sig, &g_previous, NULL);
}

static void other_handler(int sig, siginfo_t *si, void *ucv)
{
    report_crash(sig, si, ucv);
    sigaction(sig, &g_prev_other[sig], NULL);   /* re-run so Android still makes its normal crash report */
    if (sig == SIGABRT) raise(SIGABRT);
}

void android_install_fault_handler(void)
{
    static int installed;                       /* called early by the app and again by main.c */
    if (installed) return;
    installed = 1;
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

    struct sigaction so;
    memset(&so, 0, sizeof(so));
    so.sa_sigaction = other_handler;
    so.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&so.sa_mask);
    static const int sigs[] = { SIGABRT, SIGBUS, SIGILL, SIGFPE };
    for (unsigned i = 0; i < sizeof(sigs) / sizeof(sigs[0]); ++i) sigaction(sigs[i], &so, &g_prev_other[sigs[i]]);
}
