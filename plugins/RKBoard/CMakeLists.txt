# RKBoard: every RKFX plugin built in, as pedals on a board. Not made with
# rkfx_add_plugin(): it adds a Standalone, and it compiles the sources of all
# the other plugins (their own entry points left out with RKFX_BOARD).
# Plugin code and bundle ID are permanent.

juce_add_plugin(RKBoard
    COMPANY_NAME             "RKMusic"
    PLUGIN_MANUFACTURER_CODE RKMu
    PLUGIN_CODE              Brd1
    BUNDLE_ID                com.rkmusic.rkboard
    PRODUCT_NAME             "RKBoard"
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
    ${CMAKE_CURRENT_SOURCE_DIR}/PedalRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Routing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RoutingPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssignPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pedals/VolumePedal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pedals/WahPedal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pedals/WahEngine.cpp
)

# Factory presets, registered by name like every plugin's: the board's own
# (presets/*.json, board presets) and the board-only pedals'
# (pedals/presets/<Name>/*.json).
add_library(rkboard_factory_presets INTERFACE)
foreach(pedal RKBoard RKVolume RKWah)
    if(pedal STREQUAL "RKBoard")
        set(presetDir "${CMAKE_CURRENT_SOURCE_DIR}/presets")
    else()
        set(presetDir "${CMAKE_CURRENT_SOURCE_DIR}/pedals/presets/${pedal}")
    endif()
    file(GLOB pedalPresets CONFIGURE_DEPENDS "${presetDir}/*.json")
    list(SORT pedalPresets)
    juce_add_binary_data(${pedal}_FactoryPresets
        HEADER_NAME ${pedal}FactoryPresetData.h
        NAMESPACE   ${pedal}FactoryPresetData
        SOURCES     ${pedalPresets})
    target_link_libraries(rkboard_factory_presets INTERFACE ${pedal}_FactoryPresets)

    set(PLUGIN ${pedal})
    configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetList.cpp.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/${pedal}FactoryPresetList.cpp" @ONLY)
    list(APPEND RKBOARD_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/${pedal}FactoryPresetList.cpp")
endforeach()

set(RKBOARD_SOURCES ${RKBOARD_SOURCES} PARENT_SCOPE)
target_sources(RKBoard PRIVATE ${RKBOARD_SOURCES})

# Every plugin's own sources, factory preset list and preset data.
set(RKBOARD_PEDAL_SOURCES "")
foreach(plugin IN LISTS RKFX_PLUGINS)
    file(GLOB pluginSources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/plugins/${plugin}/*.cpp")
    list(APPEND RKBOARD_PEDAL_SOURCES ${pluginSources} "${CMAKE_BINARY_DIR}/plugins/${plugin}/FactoryPresetList.cpp")
    if(TARGET ${plugin}_FactoryPresets)
        target_link_libraries(RKBoard PRIVATE ${plugin}_FactoryPresets)
    endif()
endforeach()
set(RKBOARD_PEDAL_SOURCES ${RKBOARD_PEDAL_SOURCES} PARENT_SCOPE)
target_sources(RKBoard PRIVATE ${RKBOARD_PEDAL_SOURCES})

target_compile_definitions(RKBoard
    PUBLIC
        RKFX_BOARD=1
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
