/* game_platform.h - the few places where the game sources must differ between the Windows build and the
 * Android (POSIX) build. Everything else is shared code. */
#ifndef GAME_PLATFORM_H
#include <stdint.h>
#define GAME_PLATFORM_H
#if defined(XBOX_ANDROID)
#define GAME_SEP "/"
#define GAME_SEPC '/'
#define GAME_NULL_DEVICE "/dev/null"
void android_install_fault_handler(void);
void android_wait_if_paused(void);
void mercenaries_android_crash_report(const char *what, uintptr_t fault, uintptr_t host_pc);
int mercenaries_android_main(void);
#else
#define GAME_SEP "\\"
#define GAME_SEPC '\\'
#define GAME_NULL_DEVICE "NUL"
#endif
#endif
