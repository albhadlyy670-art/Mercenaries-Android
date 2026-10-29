#pragma once
#include <android/input.h>
#include <android/native_window.h>
#include <android/native_activity.h>
struct android_app; struct android_poll_source { int id; struct android_app *app; void (*process)(struct android_app *app, struct android_poll_source *source); };
struct android_app { void *userData; void (*onAppCmd)(struct android_app *app, int32_t cmd); int32_t (*onInputEvent)(struct android_app *app, AInputEvent *event); ANativeActivity *activity; ANativeWindow *window; int destroyRequested; };
enum { APP_CMD_INIT_WINDOW = 1, APP_CMD_TERM_WINDOW = 2, APP_CMD_DESTROY = 4 };
