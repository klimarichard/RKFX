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
set(RKFX_PLUGINS
    RKFreqShift
    RKFilter
    RKParaEQ
    RKGraphEQ
    RKCompress
    RKGate
    RKTremolo
    RKChorus
    RKDouble
    RKFlange
    RKPhase
    RKDelay
    RKSaturate
    RKDistort
    RKVerb
    RKLimit
    RKTransient
    RKDeEss
    RKSwell
    RKRotary
    RKVibrato
    RKVocode
    RKUtility
    RKOctave
    RKAcoustic
    RKPreamp
    RKSynth
    RKRingMod
    RKLoop
    RKVoice
    RKMultiComp
    RKRide
    RKHold
    RKPolyPitch
    RKRemap
)
foreach(plugin IN LISTS RKFX_PLUGINS)
    add_subdirectory(plugins/${plugin})
endforeach()

# The pedalboard, holding every plugin above (after them: it uses their
# factory preset lists).
add_subdirectory(plugins/RKBoard)

# The MIDI family: MIDI effects (MIDI in, MIDI out, no audio), and audio
# effects that send MIDI (RKFollow, RKTrigger). Not on the pedalboard, whose
# pedals pass audio only.
set(RKFX_MIDI_PLUGINS
    RKHarmonize
    RKArpeggiate
    RKStrum
    RKSustain
    RKConduct
    RKFollow
    RKTrigger
    RKEcho
)
foreach(plugin IN LISTS RKFX_MIDI_PLUGINS)
    add_subdirectory(plugins/${plugin})
endforeach()

# RKRemap step 0: two throwaway 64-channel test plugins (off by default).
option(RKFX_BUILD_REMAP_TEST "Build the RKRemap 64-channel test plugins" OFF)
if(RKFX_BUILD_REMAP_TEST)
    add_subdirectory(tools/remap-test)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

# Offscreen editor snapshots (optional tool)
add_subdirectory(tools)
