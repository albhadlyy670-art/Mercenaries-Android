#define _GNU_SOURCE
#include "android_signal_stack.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static pthread_barrier_t barrier;
static void *stacks[2];
static void *old_stacks[2];
static _Thread_local volatile sig_atomic_t handler_on_altstack;
static _Thread_local volatile sig_atomic_t handler_count;
static _Thread_local void *stack_base;
static _Thread_local size_t stack_size;

static void signal_handler(int signal_number)
{
    volatile unsigned char local[256u * 1024u];
    local[0] = 1;
    local[sizeof(local) - 1] = 2;
    if ((uintptr_t)&local[0] >= (uintptr_t)stack_base &&
        (uintptr_t)&local[sizeof(local) - 1] - (uintptr_t)stack_base < stack_size &&
        local[0] == 1 && local[sizeof(local) - 1] == 2)
        ++handler_on_altstack;
    ++handler_count;
    if (signal_number == SIGUSR1) raise(SIGUSR2);
}

static void *worker(void *arg)
{
    long id = (long)arg;
    stack_t stack;
    old_stacks[id] = malloc(64u * 1024u);
    assert(old_stacks[id]);
    stack_t old = { .ss_sp = old_stacks[id], .ss_size = 64u * 1024u, .ss_flags = 0 };
    assert(sigaltstack(&old, NULL) == 0);
    assert(android_install_thread_signal_stack() == 0);
    assert(android_install_thread_signal_stack() == 0); /* idempotent */
    assert(sigaltstack(NULL, &stack) == 0);
    assert(!(stack.ss_flags & SS_DISABLE));
    assert(stack.ss_size >= ANDROID_SIGNAL_STACK_SIZE);
    assert(stack.ss_sp != old.ss_sp);
    stacks[id] = stack.ss_sp;
    stack_base = stack.ss_sp;
    stack_size = stack.ss_size;
    pthread_barrier_wait(&barrier);
    assert(raise(SIGUSR1) == 0);
    assert(handler_on_altstack == 2 && handler_count == 2);
    assert(stacks[0] != stacks[1]);
    return NULL; /* TLS destructor cleans up */
}

int main(void)
{
    pthread_t t[2];
    struct sigaction action = {0};
    action.sa_handler = signal_handler;
    action.sa_flags = SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGUSR1, &action, NULL) == 0);
    assert(sigaction(SIGUSR2, &action, NULL) == 0);
    assert(pthread_barrier_init(&barrier, NULL, 2) == 0);
    for (long i = 0; i < 2; ++i) assert(pthread_create(&t[i], NULL, worker, (void *)i) == 0);
    for (int i = 0; i < 2; ++i) assert(pthread_join(t[i], NULL) == 0);
    pthread_barrier_destroy(&barrier);
    free(old_stacks[0]); free(old_stacks[1]);
    puts("signal stacks: independent, idempotent and cleaned up");
    return 0;
}
