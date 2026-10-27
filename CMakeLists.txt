cmake_minimum_required(VERSION 3.22)

project(RKFX VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
# Pinned to one release for every plugin in the suite (all plugins are upgraded
# together). The first configure downloads JUCE into build/_deps; later
# configures reuse it.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Shared core (rkfx_core_dsp + rkfx_core) and the rkfx_add_plugin() helper
# ---------------------------------------------------------------------------
add_subdirectory(core)

# ---------------------------------------------------------------------------
# Plugins
# ---------------------------------------------------------------------------
add_subdirectory(plugins/RKFreqShift)
add_subdirectory(plugins/RKFilter)
add_subdirectory(plugins/RKParaEQ)
add_subdirectory(plugins/RKGraphEQ)
add_subdirectory(plugins/RKCompress)
add_subdirectory(plugins/RKGate)
add_subdirectory(plugins/RKTremolo)
add_subdirectory(plugins/RKChorus)
add_subdirectory(plugins/RKDouble)
add_subdirectory(plugins/RKFlange)
add_subdirectory(plugins/RKPhase)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
