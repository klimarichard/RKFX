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
)
foreach(plugin IN LISTS RKFX_PLUGINS)
    add_subdirectory(plugins/${plugin})
endforeach()

# The pedalboard, holding every plugin above (after them: it uses their
# factory preset lists).
add_subdirectory(plugins/RKBoard)

# MIDI effects (MIDI in, MIDI out, no audio): the MIDI family. Not on the
# pedalboard, whose pedals are audio effects.
set(RKFX_MIDI_PLUGINS
    RKHarmonize
    RKArpeggiate
    RKStrum
    RKSustain
    RKConduct
)
foreach(plugin IN LISTS RKFX_MIDI_PLUGINS)
    add_subdirectory(plugins/${plugin})
endforeach()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

# Offscreen editor snapshots (optional tool)
add_subdirectory(tools)
