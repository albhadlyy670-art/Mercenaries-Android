# DLL plugins for Mercenaries Recompiled

Plugin Interface 1 is available in builds containing the plugin host. The original
public v1.1 executable does **not** implement it. Ship a plugin only to users with
an updated runtime **and launcher**.

Asset-only mods need no manifest. Legacy proxy DLLs still use their existing
Windows loading route. Merely adding mod.ini does not convert an old DLL:
it must implement the exported entry point in this SDK.

## Player installation

Put one mod folder directly under mods, enable it in the existing selector, and
launch modded. Restart the game after changing the selection. There is no hot
loading or unloading. Do not put dependency DLLs in their own mod folders.

The selector displays Ready (manifest checks passed), Plugin (disabled),
Error (invalid manifest or unmet requirement), or DLL (legacy/dependency files
without an official plugin manifest). Click the status, or select the row and
press F1, for details. Ready does not mean the DLL has executed or gameplay
compatibility has been established. Initialization errors name the plugin.

## Folder layout

~~~text
mods/
  Example Plugin/
    mod.ini
    example.dll
    example.ini
    helper.dll          (optional dependency)
~~~

Use unique lowercase plugin IDs. Settings remain in
plugin-settings/<id>/ beside the installation, outside mod-cache.
Each plugin's other files are staged under its own runtime/plugins/<id>/ folder.
Plugins can therefore have the same settings filenames without overwriting one
another. Configuration files (.ini, .json, .cfg, .conf, .config, .toml, .yaml,
.yml, .xml) seed persistent defaults only when no saved file exists. Updated
defaults do not overwrite player settings; migrate settings explicitly if needed.
mod.ini is metadata, never a saved setting.

Read/edit settings through host->settings_directory. Read packaged resources
through host->plugin_directory. Both are absolute UTF-8 paths, valid until
shutdown. Do not assume the current working directory is your plugin folder.
Use the Windows wide-character file APIs after converting UTF-8.

Game assets (DATAxbox, movies, game_files and existing retail paths) keep the
ordinary overlay rules, even in a plugin mod. modcompatibility.ini retains its
existing global compatibility-settings behavior. A plugin's other files stay
private; a proxy that must replace an executable-adjacent DLL belongs in a
legacy mod, not a [Plugin] package. The runtime plugins directory is reserved.

## mod.ini

Save as UTF-8, optionally with a BOM. Sections and keys are case-insensitive;
IDs are lowercase and case-sensitive. Values are unquoted. Comments occupy their
own lines beginning with ; or #. Unknown/duplicate [Plugin] keys are errors.

~~~ini
[Plugin]
Id=example
Name=Example Plugin
ApiVersion=1
Library=example.dll
; Optional comma-separated IDs:
; Requires=shared-library
; Conflicts=alternative-camera
; Exclusive=player-camera
~~~

| Key | Meaning |
| --- | --- |
| Id | Required, stable 1–64 lowercase ASCII letters/digits/underscores/hyphens; no Windows device names. Changing it creates a different settings folder. |
| Name | Optional display name; defaults to Id. The selector's main list retains the folder name. |
| ApiVersion | Required, exactly 1 for this SDK. Unknown versions are rejected before initialization. |
| Library | Required relative .dll path. No absolute paths, traversal, links or device names. Directory/filename stems use letters, digits, underscores or hyphens. |
| Requires | Optional plugin IDs that must be enabled **earlier** in load order. No automatic reorder or DLL function sharing is implied. Cyclic requirements cannot satisfy this rule. |
| Conflicts | Optional IDs that must not be enabled together; checked in either direction. Declare only known incompatibilities. |
| Exclusive | Optional resource IDs that this plugin alone may claim, e.g. player-camera. Two enabled claimants are rejected. These are cooperative declarations, not enforced native memory isolation. |

All optional lists default to empty. A mod.ini without [Plugin] does not opt in.
A missing DLL, duplicate ID, wrong API, invalid path, unmet dependency, declared
conflict or exclusive-resource collision stops a modded launch with an explanation.
A disabled broken plugin does not prevent a different selection or vanilla launch.

An ordinary file overlap is not a declared plugin incompatibility. Normal game
asset overrides still follow load order. This feature does not change DSK merging
or provide a general asset-overlap detector.

## Building the example

Use Visual Studio 2022's x64 developer command prompt:

~~~bat
cmake -S ports/mercenaries/sdk/example -B build/plugin-example -A x64
cmake --build build/plugin-example --config Release
~~~

When using the standalone SDK ZIP, run from its `sdk` folder with
`cmake -S example -B build -A x64`, then `cmake --build build --config Release`.

The output directory printed by CMake contains example.dll, mod.ini and
example.ini. Copy that complete directory into mods/Example Plugin.
The example logs initialization, its first update, periodic updates and shutdown.
It checks the retail XBE header through read_guest; it does not change gameplay.
The log is plugin-settings/plugins.log. LogIntervalMs is configurable in the
persistent example.ini (250–60000 ms).

The example builds as C with a static CRT and includes mercenaries_plugin.h.
Third-party plugins may use C++ internally, but the entry point must have C
linkage and export exactly **MercenariesPlugin_Init**. No decorated C++ export,
executable imports, RVA scans or compiler-proxy DLL are needed.

## Lifecycle and ABI

The host calls init outside DllMain after the supported retail XBE has been
validated and mapped, before its guest entry point runs. Guest objects are not
necessarily constructed yet. Inspect object signatures before changing fields,
and wait for an update when necessary.

The host passes MercPluginHost and a zeroed MercPlugin with struct_size and
api_version filled. Check the host version/size. Fill userdata, on_update and/or
on_shutdown without changing the supplied size/version. Return 1 to accept;
return 0 with a useful NUL-terminated UTF-8 error. A valid shutdown callback
supplied before an init failure is called for cleanup.

Initialization and updates run in selected load order. Shutdown runs in reverse
order, including rollback when a later initialization fails. Successfully loaded
DLLs stay resident until normal shutdown. Start no work in DllMain; join any owned
workers during shutdown before the host unloads your DLL. Process termination
or a fatal native crash cannot guarantee cleanup.

Version 1 supports one update and one shutdown callback per plugin; dispatch to
your own internal modules if needed. It deliberately does not expose arbitrary
guest-function calls, graphics-device objects, input replacement or native detours.
Future facilities need documented API versions, not guessed structure extensions.
The legacy MercenariesMod_Init/MercModHost interface used by some proxy mods is
different and is not invoked by this host.

## Update timing and guest access

Updates run on the initialization/guest owner thread at retail logical-input
checkpoints. More than one checkpoint can occur per game update. They are **not**
render-frame, vblank, fixed-rate or simulation callbacks. They can run in menus
and while simulation is paused; they may not run during blocking loads.
wall_delta_seconds is elapsed monotonic real time, not gameplay time. Throttle
periodic maintenance using time rather than counting callbacks. It is valid for
two callbacks within one millisecond to have a zero delta.

read_guest and write_guest copy bytes and return 1 on success or 0 on rejection.
They accept lower guest RAM addresses [0x00010000, 0x04000000), validate the complete
range without integer overflow, and check committed/readable/writable pages.
They reject use outside your init/update/shutdown callback, from another thread,
through another plugin's context, or with an invalid range. The API does not
expose a raw guest pointer. It does not validate your interpretation of an object,
synchronize every game worker, or prove that a write is semantically safe.

Keep callbacks short; no waits, modal UI, file scanning or expensive compilation
each update. Do not retain guest pointers or mutate native game code through
undocumented addresses. Two plugins can still intentionally change the same
guest data: coordinate with authors or declare known conflicts.

on_update returns 1 normally. Returning 0 stops further updates for that plugin
and all its dependent plugins, shows an error and logs the failure. Unrelated
plugins continue. Cleanup runs at game shutdown; DLLs are not unloaded mid-game.
Use the host logger for a detailed cause before returning 0. Do not throw
exceptions across the C ABI. Arbitrary DLL crashes cannot be safely recovered
from or sandboxed by this interface.

## Dependencies and troubleshooting

Build Windows **x64** DLLs. Bundle non-system dependency DLLs alongside the plugin
DLL; dependencies use that directory and Windows System32, not PATH or the
working directory. Prefer uniquely named dependency libraries: Windows may reuse
an already loaded dependency by name, so private folders alone cannot solve all
native DLL dependency collisions.

plugins.log is replaced on each plugin-enabled game launch. It records initialization,
callback failures and shutdown. Common errors:

- Missing entry point: implement/export MercenariesPlugin_Init.
- Windows error 193: wrong architecture or invalid DLL.
- Windows error 126: plugin or one of its dependency DLLs could not be found.
- Unsupported interface: install a supporting recomp build or rebuild against this SDK.
- Read/write rejected: verify the address/range, thread, callback lifetime and page access.
- Requires: enable the named plugin and place it earlier.
- Failure after a recomp update in an old proxy: fixed native offsets are not this API.

Do not package legacy Blood/proxy DLLs with a new manifest and expect conversion.
Authors must replace their bootstrap/detours with the entry point and callbacks.
Test fresh settings, repeated launches, changed load orders, disabled/vanilla runs,
paused gameplay and normal shutdown before distributing a plugin.

