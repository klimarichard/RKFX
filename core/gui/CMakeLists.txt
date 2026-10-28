# rk_gui: the RKMusic look, shared by RKFX and RKTracks (palette, fonts, look
# and feel, knob, switches, fader, meter, response curve).
#
# Self-contained on purpose: RKTracks pulls just this folder from the RKFX repo
# with FetchContent (SOURCE_SUBDIR core/gui), so it must not depend on anything
# else in RKFX. Include headers as "gui/LookAndFeel.h" etc.
#
# INTERFACE library like the rest of the core: each plugin compiles the sources
# itself, so there are no duplicate JUCE symbols. The consumer links
# juce_gui_basics and juce_audio_processors.

if(TARGET rk_gui)
    return()
endif()

add_library(rk_gui INTERFACE)
target_include_directories(rk_gui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_sources(rk_gui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/LookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Knob.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Switches.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ResponseCurve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Fader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GainReductionMeter.cpp
)
