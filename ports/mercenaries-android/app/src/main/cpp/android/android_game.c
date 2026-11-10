/*
 * android_game.c - runs the game on the dedicated game thread.
 *
 * The real startup sequence (load XBE, map guest memory, devices, kernel, options, controls, D3D8 device,
 * xbe_entry_point) is ports/mercenaries/src/main.c, compiled for Android as mercenaries_android_main().
 * This file only prepares the environment it reads and provides the pause hook and a fallback status
 * screen for builds without the recompiler output.
 */
#define _GNU_SOURCE
#include <stdarg.h>
#include "android_host.h"
#include "d3d11_gles_types.h"
#include <android/log.h>
#include <stdatomic.h>
#include <pthread.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void android_trace(const char *fmt, ...);

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "MercRecomp", __VA_ARGS__)

atomic_int g_android_paused;           /* set while the window is gone */
atomic_int g_android_quit;

/* Called from main.c's per-frame host service: block while backgrounded. */
void android_wait_if_paused(void)
{
    while (atomic_load(&g_android_paused) && !atomic_load(&g_android_quit)) usleep(20000);
}

void android_trace(const char *fmt, ...);
static void on_exit_trace(void) { android_trace("[app] exit() was called by the game or the process is ending normally"); }
static void trace_memory(void)
{
    FILE *f = fopen("/proc/meminfo", "r"); char l[160]; int n = 0;
    while (f && n < 3 && fgets(l, sizeof(l), f)) { size_t k = strlen(l); if (k && l[k-1]=='\n') l[k-1]=0; android_trace("[app] %s", l); ++n; }
    if (f) fclose(f);
}

static void show(const char *status, float progress)
{
    LOG("%s", status); fprintf(stderr, "[app] %s\n", status);
    android_gl_present(status, progress);
}

#ifdef MERC_HAVE_GAME
extern int mercenaries_android_main(void);
void android_install_hang_sampler(void);

/* Diagnostics: if the game seems stuck, sample where its thread is every few seconds (written to crash.log). */
static pthread_t g_game_tid;
extern volatile int g_sample_label;
#define MAX_NOTED 32
static pthread_t g_noted[MAX_NOTED];
static unsigned g_noted_tid[MAX_NOTED];
static int g_noted_n;
static pthread_mutex_t g_noted_lock = PTHREAD_MUTEX_INITIALIZER;

/* Called by CreateThread (win32_compat.c) for every guest thread. */
void android_note_thread(pthread_t t, const void *start, unsigned tid, size_t stack)
{
    Dl_info d; const char *nm = "?";
    if (dladdr(start, &d) && d.dli_sname) nm = d.dli_sname;
    pthread_mutex_lock(&g_noted_lock);
    if (g_noted_n < MAX_NOTED) { g_noted[g_noted_n] = t; g_noted_tid[g_noted_n] = tid; ++g_noted_n; }
    pthread_mutex_unlock(&g_noted_lock);
    android_trace("[thread] created xbox_tid=%u start=%s stack=%zu", tid, nm, stack);
}

static void *watchdog_thread(void *arg)
{
    (void)arg;
    sleep(8);
    for (int i = 0; i < 90; ++i) {
        g_sample_label = 0;
        pthread_kill(g_game_tid, SIGUSR2);
        usleep(60000);
        pthread_t th[MAX_NOTED]; unsigned id[MAX_NOTED]; int n;
        pthread_mutex_lock(&g_noted_lock);
        n = g_noted_n; memcpy(th, g_noted, sizeof(th)); memcpy(id, g_noted_tid, sizeof(id));
        pthread_mutex_unlock(&g_noted_lock);
        for (int k = 0; k < n; ++k) {
            g_sample_label = (int)id[k];
            pthread_kill(th[k], SIGUSR2);
            usleep(60000);
        }
        sleep(2);
    }
    return NULL;
}
#endif

int android_game_run(const char *game_dir, const char *save_dir, const char *tmp_dir)
{
    char path[1024], config[1024];
    android_install_fault_handler();            /* so even an early crash is written to crash.log */
    show("Starting", 0.05f);

    snprintf(config, sizeof(config), "%s/config", save_dir); mkdir(config, 0775);
    snprintf(path, sizeof(path), "%s/mercenaries", tmp_dir);
    setenv("XBOX_TMPDIR", tmp_dir, 1);                  /* memfd fallback location */
    setenv("MERCENARIES_GAME_DIR", game_dir, 1);        /* main.c: select_game_dir */
    setenv("MERCENARIES_SAVE_DIR", save_dir, 1);
    setenv("MERCENARIES_CONFIG_ROOT", config, 1);       /* options/controls/developer ini */
    setenv("MERCENARIES_EXE_PATH", path, 1);            /* "executable directory" lookups */
    { D3d11GlesHost h = { android_gl_present_texture, NULL }; d3d11gles_set_host(&h); }   /* back buffer -> screen */

    snprintf(path, sizeof(path), "%s/default.xbe", game_dir);
    if (access(path, R_OK) != 0) { show("default.xbe not found - re-import the disc image", 0.f); while (!atomic_load(&g_android_quit)) usleep(100000); return 1; }

#ifdef MERC_HAVE_GAME
    g_game_tid = pthread_self();
    android_install_hang_sampler();
    { pthread_t w; if (pthread_create(&w, NULL, watchdog_thread, NULL) == 0) pthread_detach(w); }
    trace_memory();
    atexit(on_exit_trace);
    android_trace("[app] entering mercenaries_android_main");
    fprintf(stderr, "[app] entering mercenaries_android_main\n");
    int rc = mercenaries_android_main();                /* does not normally return */
    LOG("mercenaries_android_main returned %d", rc);
    return rc;
#else
    show("Game code not generated: build with the recompiler output (see ANDROID_PORT.md)", 0.f);
    while (!atomic_load(&g_android_quit)) { android_gl_present("", 0.f); usleep(16000); }
    return 2;
#endif
}
