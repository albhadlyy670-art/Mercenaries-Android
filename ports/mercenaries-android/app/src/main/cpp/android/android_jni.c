/* android_jni.c - JNI entry points used by LauncherActivity. */
#include "xdvdfs_extract.h"
#include <jni.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

static atomic_int g_cancel;
typedef struct { JNIEnv *env; jobject obj; jmethodID mid; } prog_t;

static int on_progress(uint64_t done, uint64_t total, const char *path, void *u)
{
    prog_t *p = (prog_t *)u;
    jstring s = (*p->env)->NewStringUTF(p->env, path ? path : "");
    (*p->env)->CallVoidMethod(p->env, p->obj, p->mid, (jlong)done, (jlong)total, s);
    (*p->env)->DeleteLocalRef(p->env, s);
    return atomic_load(&g_cancel);
}

/* Returns "" on success, else a human-readable error. Takes ownership of fd. */
JNIEXPORT jstring JNICALL
Java_com_mercrecomp_LauncherActivity_nativeExtract(JNIEnv *env, jobject self, jint fd, jstring dest)
{
    char err[256] = "";
    const char *d = (*env)->GetStringUTFChars(env, dest, NULL);
    jclass cls = (*env)->GetObjectClass(env, self);
    prog_t p = { env, self, (*env)->GetMethodID(env, cls, "onProgress", "(JJLjava/lang/String;)V") };
    atomic_store(&g_cancel, 0);
    int rc = xiso_extract(fd, d, on_progress, &p, err, sizeof(err));
    (*env)->ReleaseStringUTFChars(env, dest, d);
    close(fd);
    if (rc == XISO_ERR_CANCELLED) snprintf(err, sizeof(err), "Cancelled");
    else if (rc != XISO_OK && !err[0]) snprintf(err, sizeof(err), "Extraction failed (%d)", rc);
    return (*env)->NewStringUTF(env, rc == XISO_OK ? "" : err);
}

JNIEXPORT void JNICALL
Java_com_mercrecomp_LauncherActivity_nativeCancel(JNIEnv *env, jobject self)
{ (void)env; (void)self; atomic_store(&g_cancel, 1); }
