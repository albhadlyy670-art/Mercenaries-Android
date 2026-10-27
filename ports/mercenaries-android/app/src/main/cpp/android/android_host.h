/* android_host.h - shared declarations between the Android glue files. */
#ifndef ANDROID_HOST_H
#define ANDROID_HOST_H
#include <android/input.h>
#include <android/native_window.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct { const char *label; float x, y, r; int id; } android_touch_btn_t;
#define ANDROID_TOUCH_BUTTONS 10
extern const android_touch_btn_t k_buttons[ANDROID_TOUCH_BUTTONS];
extern const float k_stick_cx, k_stick_cy, k_stick_r;

int  android_input_event(AInputEvent *e);
void android_input_set_screen(int w, int h);
int  android_input_overlay_pressed(int id);

/* EGL / GLES3 presentation (game thread owns the context) */
int  android_gl_attach(ANativeWindow *w);   /* create surface+context; 1 on success */
void android_gl_detach(void);               /* drop surface (app backgrounded) */
void android_gl_present(const char *status, float progress);
int  android_gl_ready(void);
void android_gl_present_texture(void *user, uint32_t tex, int w, int h, int sync_interval);  /* D3d11GlesHost.present */
extern int g_android_rendering;

void android_install_fault_handler(void);

/* game thread */
int android_game_run(const char *game_dir, const char *save_dir, const char *tmp_dir);

#ifdef __cplusplus
}
#endif
#endif
