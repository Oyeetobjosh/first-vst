#--------------------------------------------------------------------------------------
# FirstVST Delay — a VST 2.4 plug-in
#
# Builds:
#   - FirstVSTDelay  : the VST plug-in shared library
#   - vst-host-test  : a headless VST host that loads the plug-in and verifies it
#
# The VST SDK 2.4 is pulled in as a git submodule in sdk/ (see .gitmodules).
# If you prefer the official SDK, point VST2_SDK_DIR at your own copy instead.
#--------------------------------------------------------------------------------------
cmake_minimum_required (VERSION 3.16)
project (FirstVST VERSION 1.0.0 LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

#--------------------------------------------------------------------------------------
# VST SDK 2.4
#--------------------------------------------------------------------------------------
set (VST2_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk/VST2_SDK CACHE PATH "Location of the VST SDK 2.4")

if (NOT EXISTS ${VST2_SDK_DIR}/pluginterfaces/vst2.x/aeffect.h)
    message (FATAL_ERROR
        "VST SDK 2.4 not found at '${VST2_SDK_DIR}'.\n"
        "Either run 'git submodule update --init' to fetch the bundled SDK mirror,\n"
        "or download the official VST SDK 2.4 from https://www.steinberg.net/en/development/vst-sdk.html\n"
        "and pass -DVST2_SDK_DIR=<path> to cmake.")
endif ()

#--------------------------------------------------------------------------------------
# Plug-in target
#--------------------------------------------------------------------------------------
add_library (FirstVSTDelay SHARED
    src/FirstVSTDelay.cpp
    src/plugin_main.cpp
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x/audioeffect.cpp
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x/audioeffectx.cpp
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x/vstplugmain.cpp
)

target_include_directories (FirstVSTDelay PRIVATE
    ${VST2_SDK_DIR}
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x
)

# VST plug-ins must not be prefixed with "lib" and use platform-specific names.
set_target_properties (FirstVSTDelay PROPERTIES
    PREFIX ""
    OUTPUT_NAME "FirstVSTDelay"
)
if (APPLE)
    set_target_properties (FirstVSTDelay PROPERTIES SUFFIX ".vst")
elseif (WIN32)
    set_target_properties (FirstVSTDelay PROPERTIES SUFFIX ".dll")
endif ()

#--------------------------------------------------------------------------------------
# Tobjosh Reverb — 360° room-audio reverb plug-in
#
# Built from three parts:
#   src/reverb  : the FDN reverb engine (pure DSP, no VST dependencies)
#   src/gui     : the self-contained UI framework (rasterizer, controls, editor)
#   src/TobjoshReverb.* : the VST 2.4 AudioEffectX wrapper
#--------------------------------------------------------------------------------------
add_library (TobjoshReverb SHARED
    src/TobjoshReverb.cpp
    src/tobjosh_plugin_main.cpp
    src/reverb/ReverbEngine.cpp
    src/gui/Raster.cpp
    src/gui/Controls.cpp
    src/gui/ReverbUI.cpp
    src/gui/Editor.cpp
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x/audioeffect.cpp
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x/audioeffectx.cpp
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x/vstplugmain.cpp
)

target_include_directories (TobjoshReverb PRIVATE
    ${VST2_SDK_DIR}
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties (TobjoshReverb PROPERTIES
    PREFIX ""
    OUTPUT_NAME "TobjoshReverb"
)
if (APPLE)
    set_target_properties (TobjoshReverb PROPERTIES SUFFIX ".vst")
elseif (WIN32)
    set_target_properties (TobjoshReverb PROPERTIES SUFFIX ".dll")
endif ()

if (APPLE)
    # macOS editor window (Cocoa bridge, ObjC++)
    set_source_files_properties (src/gui/EditorMac.mm PROPERTIES COMPILE_FLAGS "-x objective-c++")
    target_sources (TobjoshReverb PRIVATE src/gui/EditorMac.mm)
endif ()

#--------------------------------------------------------------------------------------
# Headless test hosts
#--------------------------------------------------------------------------------------
add_executable (vst-host-test host-test/host_test.cpp)

target_include_directories (vst-host-test PRIVATE
    ${VST2_SDK_DIR}
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x
)

if (UNIX AND NOT APPLE)
    find_library (DL_LIB dl)
    if (NOT DL_LIB)
        message (FATAL_ERROR "libdl not found — required on Linux for dlopen/dlsym")
    endif ()
    target_link_libraries (vst-host-test PRIVATE ${DL_LIB})
endif ()

# Tobjosh Reverb audio host test
add_executable (tobjosh-host-test host-test/tobjosh_host_test.cpp)

target_include_directories (tobjosh-host-test PRIVATE
    ${VST2_SDK_DIR}
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x
)

if (UNIX AND NOT APPLE)
    target_link_libraries (tobjosh-host-test PRIVATE ${DL_LIB})
endif ()

# Tobjosh Reverb headless UI test (drives the editor + UI framework directly)
add_executable (ui-test host-test/ui_test.cpp
    src/reverb/ReverbEngine.cpp
    src/gui/Raster.cpp
    src/gui/Controls.cpp
    src/gui/ReverbUI.cpp
    src/gui/Editor.cpp
)

target_include_directories (ui-test PRIVATE
    ${VST2_SDK_DIR}
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Tobjosh Reverb UI screenshot tool (renders the headless editor to a PPM)
add_executable (ui-shot host-test/ui_shot.cpp
    src/reverb/ReverbEngine.cpp
    src/gui/Raster.cpp
    src/gui/Controls.cpp
    src/gui/ReverbUI.cpp
    src/gui/Editor.cpp
)

target_include_directories (ui-shot PRIVATE
    ${VST2_SDK_DIR}
    ${VST2_SDK_DIR}/public.sdk/source/vst2.x
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

#--------------------------------------------------------------------------------------
# Testing
#--------------------------------------------------------------------------------------
enable_testing ()

add_test (
    NAME firstvst-delay-host-test
    COMMAND vst-host-test $<TARGET_FILE:FirstVSTDelay>
)

add_test (
    NAME tobjosh-reverb-host-test
    COMMAND tobjosh-host-test $<TARGET_FILE:TobjoshReverb>
)

add_test (
    NAME tobjosh-reverb-ui-test
    COMMAND ui-test
)

add_custom_target (check DEPENDS vst-host-test tobjosh-host-test ui-test FirstVSTDelay TobjoshReverb
    COMMAND ${CMAKE_COMMAND} -E echo "Running FirstVSTDelay host test..."
    COMMAND vst-host-test $<TARGET_FILE:FirstVSTDelay>
    COMMAND ${CMAKE_COMMAND} -E echo "Running Tobjosh Reverb host test..."
    COMMAND tobjosh-host-test $<TARGET_FILE:TobjoshReverb>
    COMMAND ${CMAKE_COMMAND} -E echo "Running Tobjosh Reverb UI test..."
    COMMAND ui-test
)
