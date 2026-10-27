#ifndef MERCENARIES_PLUGIN_H
#define MERCENARIES_PLUGIN_H
#include <stdint.h>
#include <stddef.h>
#define MERCENARIES_PLUGIN_API_VERSION 1u
#define MERC_PLUGIN_CALL __cdecl
#define MERC_PLUGIN_EXPORT __declspec(dllexport)
#ifdef __cplusplus
extern "C" {
#endif
/* Windows x64 C ABI. Existing v1 fields/signatures must never change. */
typedef struct MercPluginUpdate {
    uint32_t struct_size, api_version;
    uint64_t update_index;
    double wall_delta_seconds; /* Real time, NOT simulation time; advances while paused. */
} MercPluginUpdate;
typedef struct MercPluginHost {
    uint32_t struct_size, api_version;
    void *context;
    const char *plugin_id;          /* UTF-8 strings, valid until shutdown. */
    const char *plugin_directory;   /* Absolute private staged files. */
    const char *settings_directory; /* Absolute persistent folder outside mod-cache. */
    void (MERC_PLUGIN_CALL *log)(void *, const char *);
    int (MERC_PLUGIN_CALL *read_guest)(void *, uint32_t, void *, size_t);
    int (MERC_PLUGIN_CALL *write_guest)(void *, uint32_t, const void *, size_t);
} MercPluginHost;
typedef struct MercPlugin {
    uint32_t struct_size, api_version;
    void *userdata;
    int (MERC_PLUGIN_CALL *on_update)(void *, const MercPluginUpdate *);
    void (MERC_PLUGIN_CALL *on_shutdown)(void *);
} MercPlugin;
/* Export with C linkage. Different from the legacy MercenariesMod_Init ABI.
 * Return 1 on success, 0 on failure; error is UTF-8/NUL terminated.
 * Fill callbacks/userdata. A supplied shutdown callback is also called if init fails.
 */
typedef int (MERC_PLUGIN_CALL *MercPluginEntry)(const MercPluginHost *,
    MercPlugin *, char *, uint32_t);
#ifdef MERC_PLUGIN_IMPLEMENTATION
MERC_PLUGIN_EXPORT int MERC_PLUGIN_CALL MercenariesPlugin_Init(
    const MercPluginHost *, MercPlugin *, char *, uint32_t);
#endif
#ifdef __cplusplus
}
#endif
#endif

