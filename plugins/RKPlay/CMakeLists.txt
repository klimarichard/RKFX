# RKPlay: the music player for intermissions and receptions (see
# docs/design/RKPlay.md). Not made with rkfx_add_plugin(): it is also a
# standalone app and reads audio files. Plugin code and bundle ID are permanent.

juce_add_plugin(RKPlay
    COMPANY_NAME             "RKMusic"
    PLUGIN_MANUFACTURER_CODE RKMu
    PLUGIN_CODE              Ply1
    BUNDLE_ID                com.rkmusic.rkplay
    PRODUCT_NAME             "RKPlay"
    ICON_BIG                 "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_big.png"    # RKMusic icon: RK over the name strip
    ICON_SMALL               "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_small.png"
    FORMATS                  AU VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE      # MIDI learn and program changes (later phases)
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE   # Delete removes a song; keyboard shortcuts later
    AU_MAIN_TYPE             kAudioUnitType_MusicEffect
    VST3_CATEGORIES          Fx
    COPY_PLUGIN_AFTER_BUILD  TRUE
)

# RKPlay's own sources (also used by its tests and the snapshot tool).
set(RKPLAY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayShow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayAnalysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayRig.cpp
    ${PROJECT_SOURCE_DIR}/host/MidiMap.cpp          # MIDI learn, shared with RKBoard and RKMix (just this file)
)

# Factory presets (presets/*.json, empty shows set up for an occasion).
file(GLOB playPresets CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.json")
list(SORT playPresets)
set(PLUGIN RKPlay)
add_library(rkplay_factory_presets INTERFACE)
if(playPresets)
    juce_add_binary_data(RKPlay_FactoryPresets
        HEADER_NAME RKPlayFactoryPresetData.h
        NAMESPACE   RKPlayFactoryPresetData
        SOURCES     ${playPresets})
    target_link_libraries(rkplay_factory_presets INTERFACE RKPlay_FactoryPresets)
    configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetList.cpp.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/RKPlayFactoryPresetList.cpp" @ONLY)
else()
    configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetListEmpty.cpp.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/RKPlayFactoryPresetList.cpp" @ONLY)
endif()
target_sources(rkplay_factory_presets INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/RKPlayFactoryPresetList.cpp")
target_compile_definitions(rkplay_factory_presets INTERFACE RKFX_FACTORY_PRESET_LIST=factoryPresetList_RKPlay)

set(RKPLAY_SOURCES ${RKPLAY_SOURCES} PARENT_SCOPE)
set(RKPLAY_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/host PARENT_SCOPE)

target_sources(RKPlay PRIVATE ${RKPLAY_SOURCES})
target_include_directories(RKPlay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/host)

target_compile_definitions(RKPlay
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(RKPlay
    PRIVATE
        rkplay_factory_presets
        rkfx_core
        juce::juce_audio_formats        # WAV, AIFF, FLAC, Ogg; MP3, M4A and ALAC through Core Audio
        juce::juce_audio_processors
        juce::juce_audio_utils          # Standalone wrapper (device settings)
        juce::juce_osc                  # remote control of the standalone app (TouchOSC)
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
