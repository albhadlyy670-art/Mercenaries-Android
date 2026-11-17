#define _GNU_SOURCE
#include "android_signal_stack.h"
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

/* sigaltstack is per-thread, not process-wide. Keep storage alive until the
 * thread exits; a single static buffer would be shared by concurrent handlers. */
static pthread_key_t stack_key;
static pthread_once_t stack_key_once = PTHREAD_ONCE_INIT;
static int stack_key_ready;

static void free_signal_stack(void *memory)
{
    stack_t disabled = { .ss_flags = SS_DISABLE };
    if (sigaltstack(&disabled, NULL) == 0)
        free(memory);
}

static void make_stack_key(void)
{
    stack_key_ready = pthread_key_create(&stack_key, free_signal_stack) == 0;
}

int android_install_thread_signal_stack(void)
{
    stack_t current;
    if (sigaltstack(NULL, &current) != 0) return -1;
    /* Respect an alternate stack already installed by the host/runtime. */
    if (!(current.ss_flags & SS_DISABLE)) return 0;
    if (pthread_once(&stack_key_once, make_stack_key) != 0 || !stack_key_ready)
        return -1;
    void *memory = malloc(64u * 1024u);
    if (!memory) return -1;
    if (pthread_setspecific(stack_key, memory) != 0) { free(memory); return -1; }
    stack_t stack = { .ss_sp = memory, .ss_size = 64u * 1024u, .ss_flags = 0 };
    if (sigaltstack(&stack, NULL) != 0) {
        pthread_setspecific(stack_key, NULL);
        free(memory);
        return -1;
    }
    return 0;
}
