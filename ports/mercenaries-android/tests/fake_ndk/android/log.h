#pragma once
enum {ANDROID_LOG_INFO=4,ANDROID_LOG_WARN=5,ANDROID_LOG_ERROR=6,ANDROID_LOG_FATAL=7};
int __android_log_print(int,const char*,const char*,...); int __android_log_write(int,const char*,const char*);
