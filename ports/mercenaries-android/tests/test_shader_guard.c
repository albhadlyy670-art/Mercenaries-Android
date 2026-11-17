#define _GNU_SOURCE
#include "android_shader_guard.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const char *g_android_crash_path;
void android_trace(const char *fmt, ...) { (void)fmt; }

static void child_done(pid_t pid)
{
    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
int main(void)
{
    char dir[] = "/tmp/merc-shader-XXXXXX", path[256];
    assert(mkdtemp(dir));
    snprintf(path, sizeof(path), "%s/crash.log", dir);
    g_android_crash_path = path;
    uint64_t safe = android_shader_pair_hash("vertex A", "fragment A");
    uint64_t bad = android_shader_pair_hash("vertex B", "fragment B");
    assert(safe != bad);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        assert(!android_shader_pair_bad(safe));
        android_shader_guard_begin(safe, "vertex-compile");
        android_shader_guard_end();
        _exit(0);
    }
    child_done(child);
    child = fork();
    assert(child >= 0);
    if (!child) {
        assert(!android_shader_pair_bad(safe)); /* successful call not blacklisted */
        android_shader_guard_begin(bad, "fragment-compile");
        _exit(0); /* simulate process death before the driver call returned */
    }
    child_done(child);
    child = fork();
    assert(child >= 0);
    if (!child) { /* consume pending marker and persist the blacklist */
        assert(android_shader_pair_bad(bad));
        assert(!android_shader_pair_bad(safe));
        _exit(0);
    }
    child_done(child);
    /* Parent has not initialized its guard, so this checks the persisted list. */
    assert(android_shader_pair_bad(bad));
    assert(!android_shader_pair_bad(safe));
    snprintf(path, sizeof(path), "%s/shader_blacklist_v7.txt", dir);
    assert(unlink(path) == 0);
    assert(rmdir(dir) == 0);
    puts("shader guard: successful calls clear markers; interrupted calls are skipped");
    return 0;
}
