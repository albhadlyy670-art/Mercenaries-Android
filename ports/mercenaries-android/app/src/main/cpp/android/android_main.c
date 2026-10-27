/*
 * android_main.c - NativeActivity entry (android_native_app_glue).
 * UI thread: lifecycle + input. Game thread: owns EGL and runs the guest.
 * Java's GameActivity writes <filesDir>/launch.cfg ("game=...\nsave=...\n")
 * before starting this activity.
 */
#include "android_host.h"
#include <android/log.h>
#include <android_native_app_glue.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "MercRecomp", __VA_ARGS__)

extern atomic_int g_android_paused, g_android_quit;

static pthread_t g_thread;
static int g_thread_started;
static pthread_mutex_t g_win_lock = PTHREAD_MUTEX_INITIALIZER;
static ANativeWindow *g_window;
static char g_game[768], g_save[768], g_tmp[768];

/* stdout/stderr of the game (and its trace output) go to logcat, tag "MercGame". */
static void *logcat_pump(void *arg)
{
    int fd = (int)(intptr_t)arg; char buf[1024]; char line[1024]; size_t n = 0; ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0)
        for (ssize_t i = 0; i < r; ++i) {
            if (buf[i] == '\n' || n == sizeof(line) - 1) { line[n] = 0; __android_log_write(ANDROID_LOG_INFO, "MercGame", line); n = 0; }
            if (buf[i] != '\n') line[n++] = buf[i];
        }
    return NULL;
}
static void redirect_stdio(void)
{
    int p[2]; if (pipe(p) != 0) return;
    dup2(p[1], 1); dup2(p[1], 2); setvbuf(stdout, NULL, _IOLBF, 0); setvbuf(stderr, NULL, _IONBF, 0);
    pthread_t t; pthread_create(&t, NULL, logcat_pump, (void *)(intptr_t)p[0]); pthread_detach(t);
}

static void *game_thread(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&g_win_lock);
    int ok = g_window && android_gl_attach(g_window);
    pthread_mutex_unlock(&g_win_lock);
    if (!ok) { LOG("EGL attach failed"); return NULL; }
    int rc = android_game_run(g_game, g_save, g_tmp);
    LOG("game thread exited rc=%d", rc);
    return NULL;
}

static void read_launch_cfg(struct android_app *app)
{
    const char *base = app->activity->internalDataPath;
    char cfg[768]; snprintf(cfg, sizeof(cfg), "%s/launch.cfg", base);
    snprintf(g_game, sizeof(g_game), "%s/game", base);
    snprintf(g_save, sizeof(g_save), "%s/saves", base);
    snprintf(g_tmp, sizeof(g_tmp), "%s", base);
    FILE *f = fopen(cfg, "r");
    if (!f) return;
    char line[800];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "game=", 5)) snprintf(g_game, sizeof(g_game), "%s", line + 5);
        else if (!strncmp(line, "save=", 5)) snprintf(g_save, sizeof(g_save), "%s", line + 5);
        else if (!strncmp(line, "tmp=", 4)) snprintf(g_tmp, sizeof(g_tmp), "%s", line + 4);
    }
    fclose(f);
}

static void on_cmd(struct android_app *app, int32_t cmd)
{
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        pthread_mutex_lock(&g_win_lock);
        g_window = app->window;
        pthread_mutex_unlock(&g_win_lock);
        if (!g_thread_started) {
            read_launch_cfg(app);
            g_thread_started = 1;
            redirect_stdio();
            pthread_attr_t attr; pthread_attr_init(&attr);
            pthread_attr_setstacksize(&attr, (size_t)64 << 20);     /* the Windows build links with a 64 MB stack */
            pthread_create(&g_thread, &attr, game_thread, NULL);
            pthread_attr_destroy(&attr);
        } else {
            pthread_mutex_lock(&g_win_lock);
            android_gl_attach(g_window);          /* returned from background */
            pthread_mutex_unlock(&g_win_lock);
            atomic_store(&g_android_paused, 0);
        }
        break;
    case APP_CMD_TERM_WINDOW:
        atomic_store(&g_android_paused, 1);
        pthread_mutex_lock(&g_win_lock);
        android_gl_detach();
        g_window = NULL;
        pthread_mutex_unlock(&g_win_lock);
        break;
    case APP_CMD_DESTROY:
        atomic_store(&g_android_quit, 1);
        break;
    default: break;
    }
}

static int32_t on_input(struct android_app *app, AInputEvent *e)
{
    (void)app;
    return android_input_event(e);
}

void android_main(struct android_app *app)
{
    app->onAppCmd = on_cmd;
    app->onInputEvent = on_input;
    while (!app->destroyRequested) {
        int events; struct android_poll_source *src;
        while (ALooper_pollAll(-1, NULL, &events, (void **)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) break;
        }
    }
    atomic_store(&g_android_quit, 1);
    /* The guest runs inside xbe_entry_point() and never unwinds; ending the
     * process is the supported way out of NativeActivity. */
    _exit(0);
}
