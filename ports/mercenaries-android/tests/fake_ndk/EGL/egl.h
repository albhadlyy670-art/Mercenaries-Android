#pragma once
#include <stdint.h>
typedef void *EGLDisplay, *EGLConfig, *EGLContext, *EGLSurface; typedef int32_t EGLint; typedef unsigned EGLBoolean; typedef void *EGLNativeWindowType;
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_DEFAULT_DISPLAY ((void*)0)
#define EGL_NONE 0x3038
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES3_BIT 0x40
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 4
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_NATIVE_VISUAL_ID 0x302E
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
EGLDisplay eglGetDisplay(void *d); EGLBoolean eglInitialize(EGLDisplay,EGLint*,EGLint*); EGLBoolean eglChooseConfig(EGLDisplay,const EGLint*,EGLConfig*,EGLint,EGLint*);
EGLBoolean eglGetConfigAttrib(EGLDisplay,EGLConfig,EGLint,EGLint*); EGLSurface eglCreateWindowSurface(EGLDisplay,EGLConfig,void*,const EGLint*);
EGLContext eglCreateContext(EGLDisplay,EGLConfig,EGLContext,const EGLint*); EGLBoolean eglMakeCurrent(EGLDisplay,EGLSurface,EGLSurface,EGLContext);
EGLBoolean eglQuerySurface(EGLDisplay,EGLSurface,EGLint,EGLint*); EGLBoolean eglSwapInterval(EGLDisplay,EGLint); EGLBoolean eglSwapBuffers(EGLDisplay,EGLSurface); EGLBoolean eglDestroySurface(EGLDisplay,EGLSurface);
