# The shared core is split in two by dependency, so the DSP side can be tested
# by a console app that links only juce_core and juce_audio_basics:
#
#   rkfx_core_dsp  dsp/, safety/, and the GUI-free parts of plugin/
#                  (output stage, preset format, preset library)
#   rkfx_core      everything else in plugin/ and gui/ (base processor and
#                  editor, parameter builder, look and feel, knobs, switches)
#
# Both are INTERFACE libraries: each plugin compiles the core sources itself,
# the usual way JUCE code is shared between targets (JUCE's own modules work
# the same way), so there are no duplicate-symbol problems with JUCE.

set(RKFX_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "RKFX core folder")

add_library(rkfx_core_dsp INTERFACE)
target_include_directories(rkfx_core_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(rkfx_core_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/OutputStage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/PresetFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/PresetLibrary.cpp
)

add_library(rkfx_core INTERFACE)
target_link_libraries(rkfx_core INTERFACE rkfx_core_dsp)
target_sources(rkfx_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/ParamBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/ProcessorBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/EditorBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/LookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Knob.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Switches.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/ResponseCurve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Fader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/GainReductionMeter.cpp
)

# ---------------------------------------------------------------------------
# rkfx_add_plugin(<Name> CODE <Abc1> BUNDLE_ID <id> [MIDI_INPUT] SOURCES <files...>)
#
# One call per plugin. Identity fields shared by the whole suite (vendor,
# manufacturer code, formats, effect type) live here so they can't drift.
# CODE and BUNDLE_ID are permanent: never change them after a release.
# ---------------------------------------------------------------------------
function(rkfx_add_plugin name)
    cmake_parse_arguments(ARG "MIDI_INPUT" "CODE;BUNDLE_ID" "SOURCES" ${ARGN})

    # MIDI input (vocoder carrier) makes the AU a music effect (aumf).
    if(ARG_MIDI_INPUT)
        set(needsMidi TRUE)
        set(auType kAudioUnitType_MusicEffect)
    else()
        set(needsMidi FALSE)
        set(auType kAudioUnitType_Effect)
    endif()

    juce_add_plugin(${name}
        COMPANY_NAME             "RKMusic"
        PLUGIN_MANUFACTURER_CODE RKMu
        PLUGIN_CODE              ${ARG_CODE}
        BUNDLE_ID                ${ARG_BUNDLE_ID}
        PRODUCT_NAME             "${name}"
        FORMATS                  AU VST3
        IS_SYNTH                 FALSE
        NEEDS_MIDI_INPUT         ${needsMidi}
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        AU_MAIN_TYPE             ${auType}
        VST3_CATEGORIES          Fx
        COPY_PLUGIN_AFTER_BUILD  TRUE
    )

    target_sources(${name} PRIVATE ${ARG_SOURCES})

    # Factory presets: every plugins/<Name>/presets/*.json is compiled in.
    # The "NN " prefix of a file name sets its menu position and is not shown.
    file(GLOB presetFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.json")
    list(SORT presetFiles)
    set(presetList "${CMAKE_CURRENT_BINARY_DIR}/FactoryPresetList.cpp")

    if(presetFiles)
        juce_add_binary_data(${name}_FactoryPresets
            HEADER_NAME FactoryPresetData.h
            NAMESPACE   FactoryPresetData
            SOURCES     ${presetFiles})
        target_link_libraries(${name} PRIVATE ${name}_FactoryPresets)

        configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetList.cpp.in" "${presetList}" COPYONLY)
    else()
        configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetListEmpty.cpp.in" "${presetList}" COPYONLY)
    endif()

    target_sources(${name} PRIVATE "${presetList}")

    target_compile_definitions(${name}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    target_link_libraries(${name}
        PRIVATE
            rkfx_core
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
endfunction()
