cmake_minimum_required(VERSION 3.22)
project(mercrecomp C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)
set(SRC  "${REPO}/src")
set(PORT "${REPO}/ports/mercenaries")

# Output of the recompiler (tools/recomp), run on a PC against YOUR disc image. Defaults to the same
# directory the Windows build reads. Without it the app builds as a shell that explains what is missing.
set(MERC_GEN_DIR "${PORT}/src/recomp/gen" CACHE PATH "Directory holding the generated recompiler C sources")
file(GLOB GEN_SRC CONFIGURE_DEPENDS "${MERC_GEN_DIR}/*.c")
if(GEN_SRC)
  set(MERC_HAVE_GAME ON)
  message(STATUS "Recompiler output found: ${MERC_GEN_DIR} (building the full game)")
else()
  set(MERC_HAVE_GAME OFF)
  message(WARNING "No recompiler output in ${MERC_GEN_DIR}: building the importer/status shell only. "
                  "Run tools/recomp on your disc image first, or pass -DMERC_GEN_DIR=...")
endif()

# ---- headers the game generates at build time (same output as the Windows CMake scripts) -----------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GAME_GEN "${CMAKE_CURRENT_BINARY_DIR}/game_gen")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/gen_game_headers.py" "${PORT}" "${GAME_GEN}"
                RESULT_VARIABLE GEN_RC OUTPUT_VARIABLE GEN_OUT ERROR_VARIABLE GEN_ERR)
if(NOT GEN_RC EQUAL 0)
  message(FATAL_ERROR "gen_game_headers.py failed:\n${GEN_OUT}\n${GEN_ERR}")
endif()
file(GLOB GEN_INPUTS "${PORT}/data/*.json" "${PORT}/src/*.lua" "${PORT}/src/*.h.in" "${PORT}/resources/*.rc" "${PORT}/scripts/Generate-Shader-Warmup.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GEN_INPUTS})

# ---- common flags / includes ------------------------------------------------------------------------
add_library(merc_common INTERFACE)
# The shadow headers (d3d11/dxgi/d3dcompiler/windows/bcrypt) must win over anything else.
target_include_directories(merc_common BEFORE INTERFACE compat/include d3d11_gles/include)
target_include_directories(merc_common INTERFACE
    compat d3d11_gles android
    "${SRC}" "${SRC}/kernel" "${SRC}/apu" "${SRC}/nv2a" "${SRC}/platform" "${SRC}/input" "${SRC}/audio" "${SRC}/d3d"
    "${PORT}/src" "${PORT}/src/recomp" "${MERC_GEN_DIR}" "${REPO}/include" "${GAME_GEN}")
target_compile_definitions(merc_common INTERFACE
    XBOX_ANDROID=1 _GNU_SOURCE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    XBOX_DEFAULT_THREAD_STACK=67108864)             # Windows build: /STACK:67108864 inherited by every thread
if(MERC_HAVE_GAME)
  target_compile_definitions(merc_common INTERFACE MERC_HAVE_GAME=1)
endif()
# Recompiled x86 relies on wrap-around integers; the Windows-derived code mixes pointer/function types
# that MSVC accepts but clang 16+ rejects by default.
target_compile_options(merc_common INTERFACE -O2 -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
    -Wno-format -Wno-incompatible-function-pointer-types -Wno-incompatible-pointer-types -Wno-int-conversion
    -Wno-implicit-int -Wno-deprecated-non-prototype -Wno-pointer-sign -Wno-unused-command-line-argument)

# ---- toolkit (kernel, APU, NV2A, D3D8-on-D3D11, D3D11-on-GLES) as a static library ---------------------
# Static, like the Windows build: members nothing references (e.g. the pushbuffer replay tool) are not linked.
file(GLOB KERNEL_SRC "${SRC}/kernel/*.c")
list(FILTER KERNEL_SRC EXCLUDE REGEX "(preview_log|mod_overlay)\\.c$")
file(GLOB APU_SRC "${SRC}/apu/*.c")
list(FILTER APU_SRC EXCLUDE REGEX "apu_xaudio2\\.c$")
file(GLOB NV2A_SRC "${SRC}/nv2a/*.c")
list(FILTER NV2A_SRC EXCLUDE REGEX "nv2a_pb_test\\.c$")
file(GLOB D3D_SRC "${SRC}/d3d/*.c")
list(FILTER D3D_SRC EXCLUDE REGEX "d3d8_(gl|compiler)\\.c$")
add_library(xbox_toolkit STATIC ${KERNEL_SRC} ${APU_SRC} ${NV2A_SRC} ${D3D_SRC}
    "${SRC}/platform/win32_compat.c"
    d3d11_gles/d3d11_gles.c d3d11_gles/hlsl2glsl.c d3d11_gles/d3d8_compiler_gles.c
    compat/winapi_extra.c compat/bcrypt_sha256.c)
target_link_libraries(xbox_toolkit PUBLIC merc_common)

# ---- the game (ports/mercenaries/src), same list as the Windows build with Android substitutions -------
set(GAME_SRC "")
if(MERC_HAVE_GAME)
  foreach(f main recomp_manual mod_compatibility mod_gameplay mod_extensions recomp_world_properties shop_config
            audio_bank_fixes ps2_upgrades recomp_options recomp_original_bugs boids mission_script_fixes recomp_ui
            recomp_controls chapter_return recomp_prompts recomp_options_menu dev_missions dev_overlay free_cam
            shader_warmup dev_menu_android mod_host_android)
    if(NOT EXISTS "${PORT}/src/${f}.c")
      message(FATAL_ERROR "game source missing: ${PORT}/src/${f}.c")
    endif()
    list(APPEND GAME_SRC "${PORT}/src/${f}.c")
  endforeach()
  list(APPEND GAME_SRC ${GEN_SRC})
endif()

# ---- Android glue --------------------------------------------------------------------------------------
set(NDK_GLUE "${ANDROID_NDK}/sources/android/native_app_glue")
add_library(app_glue STATIC "${NDK_GLUE}/android_native_app_glue.c")
target_include_directories(app_glue PUBLIC "${NDK_GLUE}")

add_library(mercrecomp SHARED
    android/android_main.c android/android_game.c android/android_gl.c android/android_input.c
    android/android_audio.c android/android_fault.c android/android_jni.c android/xdvdfs_extract.c
    android/preview_log_android.c
    ${GAME_SRC})
if(NOT MERC_HAVE_GAME)
  target_sources(mercrecomp PRIVATE android/shell_stubs.c)   # inert versions of symbols the game normally defines
endif()
target_link_libraries(mercrecomp PRIVATE merc_common xbox_toolkit app_glue android log EGL GLESv3 aaudio m)
# Keep ANativeActivity_onCreate; align segments to 16 KB (required on Android 15+ devices with 16 KB pages).
set_target_properties(mercrecomp PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate -Wl,-z,max-page-size=16384")
