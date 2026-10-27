cmake_minimum_required(VERSION 3.20)
project(mercenaries_example_plugin C)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build this plugin for Windows x64.")
endif()
add_library(example SHARED example.c)
set_target_properties(example PROPERTIES PREFIX "" C_STANDARD 11
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MSVC)
    target_compile_options(example PRIVATE /W4 /WX /utf-8)
endif()
add_custom_command(TARGET example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/mod.ini" "$<TARGET_FILE_DIR:example>/mod.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/example.ini" "$<TARGET_FILE_DIR:example>/example.ini"
    COMMAND ${CMAKE_COMMAND} -E echo "Plugin folder: $<TARGET_FILE_DIR:example>")

