# RKBoard: every RKFX plugin built in, as pedals on a board. Not made with
# rkfx_add_plugin(): it adds a Standalone, and it compiles the sources of all
# the other plugins through the pedal host (host/).
# Plugin code and bundle ID are permanent.

juce_add_plugin(RKBoard
    COMPANY_NAME             "RKMusic"
    PLUGIN_MANUFACTURER_CODE RKMu
    PLUGIN_CODE              Brd1
    BUNDLE_ID                com.rkmusic.rkboard
    PRODUCT_NAME             "RKBoard"
    ICON_BIG                 "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_big.png"    # RKMusic icon: RK over the name strip
    ICON_SMALL               "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_small.png"  # the small-size version
    FORMATS                  AU VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE      # vocoder carrier, scenes and MIDI learn (later)
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE             kAudioUnitType_MusicEffect
    VST3_CATEGORIES          Fx
    COPY_PLUGIN_AFTER_BUILD  TRUE
)

# The board's own sources (also used by RKBoardTests and the snapshot tool).
set(RKBOARD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/BoardProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BoardEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Routing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RoutingPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssignPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MidiLearn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PitchDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TunerPanel.cpp
)

# The board's own factory presets (presets/*.json, board presets), registered
# by name like every plugin's. The pedals and their presets come from the
# shared pedal host (host/).
add_library(rkboard_factory_presets INTERFACE)
file(GLOB boardPresets CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.json")
list(SORT boardPresets)
juce_add_binary_data(RKBoard_FactoryPresets
    HEADER_NAME RKBoardFactoryPresetData.h
    NAMESPACE   RKBoardFactoryPresetData
    SOURCES     ${boardPresets})
target_link_libraries(rkboard_factory_presets INTERFACE RKBoard_FactoryPresets rkfx_pedal_host)
set(PLUGIN RKBoard)
configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetList.cpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/RKBoardFactoryPresetList.cpp" @ONLY)
list(APPEND RKBOARD_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/RKBoardFactoryPresetList.cpp")

set(RKBOARD_SOURCES ${RKBOARD_SOURCES} PARENT_SCOPE)
target_sources(RKBoard PRIVATE ${RKBOARD_SOURCES})

target_compile_definitions(RKBoard
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(RKBoard
    PRIVATE
        rkboard_factory_presets
        rkfx_core
        juce::juce_audio_processors
        juce::juce_audio_utils          # Standalone wrapper (device settings)
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
