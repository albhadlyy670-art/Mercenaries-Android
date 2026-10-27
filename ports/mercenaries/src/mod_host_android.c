/* Android replacements for plugin_host.cpp and mod_saves.cpp.
 *
 * Both implement desktop-only mod infrastructure: plugin_host.cpp loads Windows DLL plugins chosen in the
 * separate mod-selector application, and mod_saves.cpp tracks permadeath-mod saves through the Windows
 * file-handle observer. Neither exists on Android (no DLLs, no selector, no mod profiles).
 *
 * The behavior here is exactly what the Windows code does when no mod is selected:
 *   - the plugin host returns success having loaded nothing (MERCENARIES_PLUGIN_LIST unset),
 *   - there is no permadeath profile, so no save is ever deleted. */
#include "plugin_host.h"
#include "mod_gameplay.h"

int merc_plugins_initialize(void *guest_base, size_t guest_bytes) { (void)guest_base; (void)guest_bytes; return 1; }
int merc_plugins_update(void) { return 1; }
void merc_plugins_shutdown(void) {}
const char *merc_plugins_error(void) { return ""; }
void merc_plugins_show_error(void) {}

void recomp_mod_saves_init(const char *root) { (void)root; }
int recomp_mod_delete_latest_save(void) { return 0; }
