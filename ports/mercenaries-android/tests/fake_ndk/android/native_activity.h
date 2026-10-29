#pragma once
typedef struct ANativeActivity { void *callbacks; void *vm; void *env; void *clazz; const char *internalDataPath; const char *externalDataPath; } ANativeActivity;
