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
# Headless test host
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

#--------------------------------------------------------------------------------------
# Testing
#--------------------------------------------------------------------------------------
enable_testing ()

add_test (
    NAME firstvst-delay-host-test
    COMMAND vst-host-test $<TARGET_FILE:FirstVSTDelay>
)

add_custom_target (check DEPENDS vst-host-test FirstVSTDelay
    COMMAND ${CMAKE_COMMAND} -E echo "Running FirstVSTDelay host test..."
    COMMAND vst-host-test $<TARGET_FILE:FirstVSTDelay>
)
