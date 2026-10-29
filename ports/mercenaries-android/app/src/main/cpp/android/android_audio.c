/*
 * android_audio.c - AAudio implementation of apu/apu_xaudio2.h.
 *
 * The APU worker thread calls xa2_submit_samples() with 256 stereo frames
 * (48 kHz, int16) per MCPX output frame. We queue them in a single-producer /
 * single-consumer ring; AAudio's real-time callback drains it, writing
 * silence on underrun. The ring holds 16 frames (~85 ms) which absorbs
 * Android scheduler jitter without adding visible lag.
 */
#include "apu/apu_xaudio2.h"
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "MercRecomp", __VA_ARGS__)
#define RATE 48000
#define CH 2
#define FRAME_SAMPLES 256
#define RING_FRAMES 16

static int16_t g_ring[RING_FRAMES][FRAME_SAMPLES * CH];
static atomic_uint g_head;      /* producer index (monotonic) */
static atomic_uint g_tail;      /* consumer index (monotonic) */
static unsigned g_tail_off;     /* samples already consumed from the tail frame */
static AAudioStream *g_stream;
static atomic_int g_active;
static atomic_uint g_underruns;

static aaudio_data_callback_result_t on_audio(AAudioStream *s, void *user, void *data, int32_t frames)
{
    (void)s; (void)user;
    int16_t *out = (int16_t *)data;
    int32_t need = frames * CH;
    while (need > 0) {
        unsigned head = atomic_load_explicit(&g_head, memory_order_acquire);
        unsigned tail = atomic_load_explicit(&g_tail, memory_order_relaxed);
        if (head == tail) {                           /* underrun: pad with silence */
            memset(out, 0, (size_t)need * sizeof(int16_t));
            atomic_fetch_add(&g_underruns, 1);
            break;
        }
        const int16_t *src = g_ring[tail % RING_FRAMES] + g_tail_off;
        int32_t avail = FRAME_SAMPLES * CH - (int32_t)g_tail_off;
        int32_t n = need < avail ? need : avail;
        memcpy(out, src, (size_t)n * sizeof(int16_t));
        out += n; need -= n; g_tail_off += (unsigned)n;
        if (g_tail_off >= FRAME_SAMPLES * CH) {
            g_tail_off = 0;
            atomic_store_explicit(&g_tail, tail + 1, memory_order_release);
        }
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

/* AAudio forbids closing a stream from its own error callback, so the
 * reopen (headphones plugged/unplugged) runs on a short-lived thread. */
static void *reopen_thread(void *arg)
{
    (void)arg;
    xa2_shutdown();
    xa2_init();
    return NULL;
}

static void on_error(AAudioStream *s, void *user, aaudio_result_t err)
{
    (void)s; (void)user;
    LOG("AAudio stream error %d - reopening", err);
    atomic_store(&g_active, 0);
    pthread_t t;
    if (pthread_create(&t, NULL, reopen_thread, NULL) == 0) pthread_detach(t);
}

int xa2_init(void)
{
    if (atomic_load(&g_active)) return 1;
    AAudioStreamBuilder *b = NULL;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return 0;
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(b, RATE);
    AAudioStreamBuilder_setChannelCount(b, CH);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
#if __ANDROID_API__ >= 28            /* setUsage is API 28+; the app supports API 26 */
    AAudioStreamBuilder_setUsage(b, AAUDIO_USAGE_GAME);
#endif
    AAudioStreamBuilder_setDataCallback(b, on_audio, NULL);
    AAudioStreamBuilder_setErrorCallback(b, on_error, NULL);
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &g_stream);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK) { LOG("AAudio open failed: %s", AAudio_convertResultToText(r)); return 0; }

    atomic_store(&g_head, 0); atomic_store(&g_tail, 0); g_tail_off = 0;
    if (AAudioStream_requestStart(g_stream) != AAUDIO_OK) {
        AAudioStream_close(g_stream); g_stream = NULL; return 0;
    }
    atomic_store(&g_active, 1);
    LOG("AAudio started: %d Hz, burst %d frames", AAudioStream_getSampleRate(g_stream),
        AAudioStream_getFramesPerBurst(g_stream));
    return 1;
}

void xa2_shutdown(void)
{
    atomic_store(&g_active, 0);
    if (g_stream) {
        AAudioStream_requestStop(g_stream);
        AAudioStream_close(g_stream);
        g_stream = NULL;
    }
}

int xa2_is_active(void) { return atomic_load(&g_active); }

int xa2_submit_samples(const int16_t *samples, int num_samples)
{
    if (!atomic_load(&g_active) || !samples) return 0;
    /* num_samples counts stereo frames (256); guard against other sizes. */
    int frames = num_samples;
    if (frames != FRAME_SAMPLES) {
        int total = frames * CH, done = 0;
        while (done < total) {
            int16_t tmp[FRAME_SAMPLES * CH] = {0};
            int n = total - done < FRAME_SAMPLES * CH ? total - done : FRAME_SAMPLES * CH;
            memcpy(tmp, samples + done, (size_t)n * sizeof(int16_t));
            if (!xa2_submit_samples(tmp, FRAME_SAMPLES)) return 0;
            done += n;
        }
        return 1;
    }
    unsigned head = atomic_load_explicit(&g_head, memory_order_relaxed);
    unsigned tail = atomic_load_explicit(&g_tail, memory_order_acquire);
    if (head - tail >= RING_FRAMES) return 0;         /* full: caller drops/waits */
    memcpy(g_ring[head % RING_FRAMES], samples, sizeof(g_ring[0]));
    atomic_store_explicit(&g_head, head + 1, memory_order_release);
    return 1;
}

int xa2_get_buffer_size(void) { return FRAME_SAMPLES; }
int xa2_get_queued_buffers(void)
{
    return (int)(atomic_load(&g_head) - atomic_load(&g_tail));
}
int xa2_get_queue_capacity(void) { return RING_FRAMES; }
