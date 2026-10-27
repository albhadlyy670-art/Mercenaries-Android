/* Android build of dev_menu.c.
 *
 * The Windows developer panel (F9) is a native Win32 window of list boxes and combo boxes; there is no
 * equivalent on a phone. This file keeps everything the rest of the game links against: the vehicle/troop
 * catalogs, the allowed/logging switches read from developer.ini, and the atomic request queues the retail
 * thread drains (spawn, boids, faction relations, battle flags). With no panel, nothing enqueues spawn
 * or boids requests, exactly like the Windows build with the developer menu disabled (its default). */
#include "dev_menu.h"
#include "dev_battle.h"
#include "dev_weapons.h"
#include "dev_factions.h"
#include "dev_missions.h"
#include "free_cam.h"
#include "recomp_controls.h"
#include "dev_overlay.h"
#include "xinput_xbox.h"
#include <stdio.h>
#include <string.h>

static const DevVehicle vehicles[] = {
#include "dev_vehicle_catalog.inc"
};
static const DevVehicle troops[] = {
#include "dev_troop_catalog.inc"
};
static volatile LONG battle_flags;
static volatile LONG pending = -1;
static volatile LONG boids_pending;
static volatile LONG relation_pending;
static void (*display_callback)(int);

unsigned recomp_dev_battle_flags(void) { return (unsigned)InterlockedCompareExchange(&battle_flags, 0, 0); }
void recomp_dev_battle_set(unsigned flags) { InterlockedExchange(&battle_flags, (LONG)(flags & 3u)); }
unsigned recomp_dev_troop_count(void) { return (unsigned)(sizeof(troops) / sizeof(troops[0])); }
const DevVehicle *recomp_dev_troop_at(unsigned i) { return i < recomp_dev_troop_count() ? troops + i : NULL; }
unsigned recomp_dev_vehicle_count(void) { return (unsigned)(sizeof(vehicles) / sizeof(vehicles[0])); }
const DevVehicle *recomp_dev_vehicle_at(unsigned i) { return i < recomp_dev_vehicle_count() ? vehicles + i : NULL; }

void recomp_dev_menu_init(HWND owner, void (*bookmark)(void)) { (void)owner; (void)bookmark; }
void recomp_dev_menu_set_display_callback(void (*callback)(int)) { display_callback = callback; }

/* developer.ini lives in the config root the Android launcher exports as MERCENARIES_CONFIG_ROOT. */
static int developer_setting(const char *name)
{
    char root[512], path[560];
    DWORD length = GetEnvironmentVariableA("MERCENARIES_CONFIG_ROOT", root, sizeof(root));
    if (!length || length >= sizeof(root)) return 0;
    snprintf(path, sizeof(path), "%s/developer.ini", root);
    return GetPrivateProfileIntA("Developer", name, 0, path) == 1;
}
int recomp_dev_menu_allowed(void)
{
    static int allowed = -1;
    if (allowed < 0) allowed = developer_setting("developer_menu");
    return allowed;
}
int recomp_developer_logging_enabled(void) { return developer_setting("logging") || getenv("MERCENARIES_DEVELOPER_LOGGING") != NULL; }

void recomp_dev_menu_toggle(void) {}                       /* no panel */
void recomp_dev_freecam_toggle(void) { if (recomp_dev_menu_allowed()) recomp_freecam_set(!recomp_freecam_enabled()); }
int recomp_dev_menu_visible(void) { return 0; }
int recomp_dev_menu_camera_input(int keyboard) { (void)keyboard; return 0; }

int recomp_dev_take_boids(void) { return InterlockedExchange(&boids_pending, 0) != 0; }
void recomp_dev_request_boids(void) { InterlockedExchange(&boids_pending, 1); }
int recomp_dev_take_spawn(unsigned *i) { LONG command = InterlockedExchange(&pending, -1); if (command < 0) return 0; *i = (unsigned)command; return 1; }

int recomp_dev_request_relation(unsigned first, unsigned second, unsigned relation)
{
    unsigned command = dev_relation_command(first, second, relation);
    return command && recomp_dev_menu_allowed() && InterlockedCompareExchange(&relation_pending, (LONG)command, 0) == 0;
}
unsigned recomp_dev_take_relation(void) { return (unsigned)InterlockedExchange(&relation_pending, 0); }

/* Results that the Windows panel shows in its status line go to the log instead. */
void recomp_dev_relation_result(int success, const char *message) { fprintf(stderr, "[DEV] relation %s: %s\n", success ? "ok" : "failed", message ? message : ""); }
void recomp_dev_spawn_progress(const char *message) { fprintf(stderr, "[DEV] %s\n", message ? message : ""); }
void recomp_dev_spawn_result(int success, const char *message) { fprintf(stderr, "[DEV] spawn %s: %s\n", success ? "ok" : "failed", message ? message : ""); }
void recomp_dev_transition_result(int success, const char *message) { fprintf(stderr, "[DEV] transition %s: %s\n", success ? "ok" : "failed", message ? message : ""); }
