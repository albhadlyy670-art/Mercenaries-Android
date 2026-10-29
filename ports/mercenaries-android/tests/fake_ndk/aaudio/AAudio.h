#pragma once
#include <stdint.h>
typedef struct AAudioStreamStruct AAudioStream; typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder; typedef int32_t aaudio_result_t;
typedef int32_t aaudio_data_callback_result_t;
enum { AAUDIO_OK = 0, AAUDIO_DIRECTION_OUTPUT = 0, AAUDIO_FORMAT_PCM_I16 = 1, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY = 12, AAUDIO_SHARING_MODE_SHARED = 1, AAUDIO_USAGE_GAME = 14, AAUDIO_CALLBACK_RESULT_CONTINUE = 0 };
typedef aaudio_data_callback_result_t (*AAudioStream_dataCallback)(AAudioStream *s, void *u, void *data, int32_t n);
typedef void (*AAudioStream_errorCallback)(AAudioStream *s, void *u, aaudio_result_t e);
aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder **b); const char *AAudio_convertResultToText(aaudio_result_t r);
void AAudioStreamBuilder_setDirection(AAudioStreamBuilder*,int); void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder*,int32_t); void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder*,int32_t);
void AAudioStreamBuilder_setFormat(AAudioStreamBuilder*,int); void AAudioStreamBuilder_setPerformanceMode(AAudioStreamBuilder*,int); void AAudioStreamBuilder_setSharingMode(AAudioStreamBuilder*,int);
void AAudioStreamBuilder_setDataCallback(AAudioStreamBuilder*,AAudioStream_dataCallback,void*); void AAudioStreamBuilder_setErrorCallback(AAudioStreamBuilder*,AAudioStream_errorCallback,void*);
aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder*,AAudioStream**); aaudio_result_t AAudioStreamBuilder_delete(AAudioStreamBuilder*);
aaudio_result_t AAudioStream_requestStart(AAudioStream*); aaudio_result_t AAudioStream_requestStop(AAudioStream*); aaudio_result_t AAudioStream_close(AAudioStream*);
int32_t AAudioStream_getSampleRate(AAudioStream*); int32_t AAudioStream_getFramesPerBurst(AAudioStream*);
#if __ANDROID_API__ >= 28
void AAudioStreamBuilder_setUsage(AAudioStreamBuilder*,int);
#endif
