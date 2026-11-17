#define _GNU_SOURCE
#include "android_signal_stack.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>

static pthread_barrier_t barrier;
static void *stacks[2];
static _Thread_local volatile sig_atomic_t handler_on_altstack;
static _Thread_local void *stack_base;
static _Thread_local size_t stack_size;

static void signal_handler(int signal_number)
{
    char local;
    (void)signal_number;
    handler_on_altstack = (char *)&local >= (char *)stack_base &&
        (char *)&local < (char *)stack_base + stack_size;
}

static void *worker(void *arg)
{
    long id = (long)arg;
    stack_t stack;
    assert(android_install_thread_signal_stack() == 0);
    assert(android_install_thread_signal_stack() == 0); /* idempotent */
    assert(sigaltstack(NULL, &stack) == 0);
    assert(!(stack.ss_flags & SS_DISABLE));
    assert(stack.ss_size >= 64u * 1024u);
    stacks[id] = stack.ss_sp;
    stack_base = stack.ss_sp;
    stack_size = stack.ss_size;
    pthread_barrier_wait(&barrier);
    assert(raise(SIGUSR1) == 0);
    assert(handler_on_altstack);
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
    assert(pthread_barrier_init(&barrier, NULL, 2) == 0);
    for (long i = 0; i < 2; ++i) assert(pthread_create(&t[i], NULL, worker, (void *)i) == 0);
    for (int i = 0; i < 2; ++i) assert(pthread_join(t[i], NULL) == 0);
    pthread_barrier_destroy(&barrier);
    puts("signal stacks: independent, idempotent and cleaned up");
    return 0;
}
