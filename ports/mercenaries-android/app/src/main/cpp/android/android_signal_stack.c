#define _GNU_SOURCE
#include "android_signal_stack.h"
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

/* The SIGSEGV MMIO handler calls the GPU and potentially the driver on this
 * stack, so the tiny signal stacks installed by some Android runtimes are not
 * sufficient. Reserve virtual memory lazily, with a guard page at each end. */
struct signal_stack {
    void *mapping;
    size_t mapping_size;
    stack_t previous;
};
static pthread_key_t stack_key;
static pthread_once_t stack_key_once = PTHREAD_ONCE_INIT;
static int stack_key_ready;

static void free_signal_stack(void *ptr)
{
    struct signal_stack *owned = ptr;
    if (sigaltstack(&owned->previous, NULL) == 0) {
        munmap(owned->mapping, owned->mapping_size);
        free(owned);
    }
    /* If restoration fails, do not unmap a stack still installed in the OS. */
}
static void make_stack_key(void)
{
    stack_key_ready = pthread_key_create(&stack_key, free_signal_stack) == 0;
}

int android_install_thread_signal_stack(void)
{
    stack_t previous;
    if (sigaltstack(NULL, &previous) != 0) return -1;
    if (previous.ss_flags & SS_ONSTACK) return -1;
    if (!(previous.ss_flags & SS_DISABLE) && previous.ss_size >= ANDROID_SIGNAL_STACK_SIZE)
        return 0;
    if (pthread_once(&stack_key_once, make_stack_key) != 0 || !stack_key_ready)
        return -1;
    long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) return -1;
    size_t usable = ((size_t)ANDROID_SIGNAL_STACK_SIZE + (size_t)page - 1u) / (size_t)page * (size_t)page;
    size_t total = usable + 2u * (size_t)page;
    struct signal_stack *owned = malloc(sizeof(*owned));
    if (!owned) return -1;
    void *mapping = mmap(NULL, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) { free(owned); return -1; }
    void *base = (char *)mapping + page;
    if (mprotect(base, usable, PROT_READ | PROT_WRITE) != 0) {
        munmap(mapping, total); free(owned); return -1;
    }
    owned->mapping = mapping; owned->mapping_size = total; owned->previous = previous;
    if (pthread_setspecific(stack_key, owned) != 0) {
        munmap(mapping, total); free(owned); return -1;
    }
    stack_t stack = { .ss_sp = base, .ss_size = usable, .ss_flags = 0 };
    if (sigaltstack(&stack, NULL) != 0) {
        pthread_setspecific(stack_key, NULL);
        munmap(mapping, total); free(owned); return -1;
    }
    return 0;
}
