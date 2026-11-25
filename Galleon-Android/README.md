# Galleon for Android
An unofficial native Android port of the original Xbox version of *Galleon*, with touch controls, gyroscope aiming, controller support and extended graphics options.

The game's x86 code is statically recompiled ahead of time to C++ with [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp), then built for arm64. Drawing goes through Vulkan, and windowing, input and audio through SDL3. No emulator is involved.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build.

## Installation
1. Install the APK.
2. Open **Galleon Android** and allow **all files access** when it asks (on Android 10, the **storage** permission).
3. Copy the game into `/storage/emulated/0/Galleon`, or into a `Galleon` folder at the top of your SD card: either as a disc image (`.iso`) or as `default.xbe` with its data folders. The app creates the folder on the phone's storage once access is allowed, and starts the game as soon as it finds the files.

If you don't grant that access, `Android/data/com.racas.galleon/files/assets` works too.

Settings (`galleon.toml`, `android.toml`, `layout.toml`), saves (`hdd`), logs and the shader cache are stored in `Android/data/com.racas.galleon/files`.

## Features
The settings are in the port's settings panel, opened with Android's back button, the gear button on the touch controls, or F10 on a keyboard. The panel has five tabs: **Graphics**, **Touch**, **General**, **Keyboard** and **Credits**. Android's back button opens and closes the panel.

> [!IMPORTANT]
> Galleon's x86 image has not been mapped beyond the XDK library surface the host replaces. The original game's own features that this port does **not** have: boot-time skips, an in-game options menu hook, cheats (unlimited ammo, god mode, weapon and level unlocks), an FPS counter, and Indonesian game strings. The graphics and input options below all work host-side and do not depend on the game's internals.

### Graphics
- **Display**
  - **Render Resolution**: 20% to 200% of the screen's height, or leave it to **Dynamic Resolution**.
  - **Screen Filter**: none, CRT (light or strong), sharpen, vibrant, noir, sepia, night vision or VHS.
- **Performance**
  - **V-Sync**, off by default.
  - **Max Frame Rate**: unlimited by default, or capped at the display's refresh rate or a custom value.
  - **Dynamic Resolution**: drops the render resolution when frame rate falls under the target and raises it again once it holds for a configurable time, within configurable minimum and maximum.
- **Quality**
  - **Anisotropic Filtering**, applied to all textures.
  - **Bloom**, **Fog**, **Anti-Aliasing** (off, FXAA, or SSAA 2x) and **HDR** output (on HDR displays).

### Touch Controls
- **Touch**: the left side of the screen is a movement stick, and dragging on the right turns the view. On-screen buttons cover fire, aim, reload, use, melee, grenades, crouch, weapon switching and the suppressor. Dragging from a fire button turns the view while firing.
- **Menus**: a D-pad and a back button move through the menus; tap an arrow to step one row, or hold it to keep going.
- **Touch Sensitivity**: how fast a drag turns the view, separate from the mouse's sensitivity.
- **Controls Layout**: pick a preset (*Default*, *3 Finger* or *4 Finger*) and set the controls' opacity and overall size.
- **Button Style**: *Xbox* (the skin's icons with the Xbox controller's A, B, X, Y and other button marks), *Clean* (the same icons without them) or *Classic* (the original drawn icons).
- **Edit Controls Position**: drag each control where you want it. To resize one, select it and tap **−** or **+**, or pinch it with a second finger.
- **Motion Controls**: aim with the gyroscope by turning the phone, or a DualShock 4 or DualSense on Android 12 and newer, either always or only while firing, with adjustable sensitivity. The controller's gyroscope is used while you play with the controller, the phone's while you use the touch screen.

### Other Input
- **Controller**: any controller SDL3 supports, with rumble. Controllers that some phones pass on as a virtual device (a DualShock 4 on MIUI, for example) work too.
  - **Stick Deadzone and Sensitivity**, under *Touch → Controller*, for each stick: a radial dead zone from 0% to 50% of the travel (0% leaves the game's own), past which the travel runs from rest to full again, scaled by a sensitivity from 0.1 to 3.0.
- **Keyboard and Mouse**, with rebindable controls, mouse sensitivity and invert look. In play the mouse is captured as on a PC: no pointer on the screen, and the view keeps turning past the screen's edge.

### General
- **Language**: English, French, German, Italian, Spanish or Indonesian for the port's own text. Galleon's disc has not been checked for localized strings, so the game's own strings stay in English; only the panel follows this setting.
- **Audio Output**: automatic, mono, stereo or 5.1 surround.
- **Check for Updates**: the app checks GitHub for a new release each time it opens. This button checks right away.
- **Background Pause**: the game and its sound pause while the app is in the background.

## System Requirements
- **Android** 10 or newer, arm64.
- **GPU** with a Vulkan 1.1 driver, for example Adreno 6xx or newer, Mali-G7x or newer, or Xclipse.
- **Memory**: 4 GB.
- **Storage**: 2 GB for the game files and saves.

## Building
Building requires a Windows PC.

### 1. Clone the repository
```
git clone https://github.com/albhadlyy670-art/Mercenaries-Android.git
```

### 2. Add the game files
Put `default.xbe` from a Galleon release in `assets/`, or put a Galleon disc image (`.iso`) in `assets/` or `iso/`. The build extracts `default.xbe` from the image.

### 3. Install the dependencies
- [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) with *Desktop development with C++*. The recompiler runs on the PC.
- [Android Studio](https://developer.android.com/studio). In its SDK Manager, install Android SDK Platform 34, NDK `28.2.13676358` and CMake `3.31.6`.
- [Python](https://www.python.org/), needed only to extract `default.xbe` from a disc image.

### 4. Build
Run `build.bat`. It builds the recompiler, recompiles the xbe into `generated/game`, then builds `app/build/outputs/apk/release/app-release.apk` with Gradle. The first build compiles the whole game for arm64 and takes 30 to 90 minutes.

Install the APK with:
```
adb install -r app/build/outputs/apk/release/app-release.apk
```

## Credits
- [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) by TGP482: the static recompiler and the portable runtime (Xbox kernel, XDK libraries and Direct3D 8) this port is built on.
- [Black for Android](https://github.com/RanggaCasper/Black-Android) by Rangga Casper: this port was retargeted from it. Huge thanks for the original work.
- [SDL](https://github.com/libsdl-org/SDL), [glslang](https://github.com/KhronosGroup/glslang), [toml++](https://github.com/marzer/tomlplusplus) and [Dear ImGui](https://github.com/ocornut/imgui).
- [bagasromadon](https://github.com/bagasromadon): the touch controls' button skin.
- [xemu](https://github.com/xemu-project/xemu) and [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded), which XboxXBE-Recomp used as references for Xbox hardware and API behavior.

*Galleon* is a trademark of its respective owners. This project is not affiliated with or endorsed by them.
