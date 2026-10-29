/* shell_stubs.c - built ONLY when no recompiler output is present (MERC_HAVE_GAME off).
 *
 * The toolkit references a few symbols that the game code (recomp_manual.c / main.c) and the recompiler
 * output normally define. The importer/status shell links without the game, so provide inert versions. */
#include <android/log.h>
#include <stdint.h>

typedef void (*recomp_func_t)(void);

volatile uint32_t g_mercenaries_gameplay_capture_active;
volatile uint32_t g_mercenaries_hq_briefing_active;
volatile uint32_t g_mercenaries_roadblock_model_draw_active;
volatile uint32_t g_mercenaries_standup_complete;
volatile uint32_t g_recomp_func_trace_armed;

recomp_func_t recomp_lookup(uint32_t xbox_va) { (void)xbox_va; return 0; }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { (void)xbox_va; return 0; }

void mercenaries_android_crash_report(const char *what, uintptr_t fault, uintptr_t host_pc)
{
    __android_log_print(ANDROID_LOG_FATAL, "MercRecomp", "%s fault=%p host_pc=%p", what, (void *)fault, (void *)host_pc);
}
