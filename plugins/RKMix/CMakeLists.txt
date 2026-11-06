# RKMix: the mixing console. Not made with rkfx_add_plugin(): its inserts are
# every RKFX plugin, built in through the pedal host (host/), as RKBoard's
# pedals are. Plugin code and bundle ID are permanent.

juce_add_plugin(RKMix
    COMPANY_NAME             "RKMusic"
    PLUGIN_MANUFACTURER_CODE RKMu
    PLUGIN_CODE              Mix1
    BUNDLE_ID                com.rkmusic.rkmix
    PRODUCT_NAME             "RKMix"
    ICON_BIG                 "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_big.png"    # RKMusic icon: RK over the name strip
    ICON_SMALL               "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_small.png"
    FORMATS                  AU VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE      # inserts with MIDI input; scenes by program change
    NEEDS_MIDI_OUTPUT        TRUE      # what RKTrigger and RKFollow inserts send; MIDI thru
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE             kAudioUnitType_MusicEffect
    VST3_CATEGORIES          Fx
    COPY_PLUGIN_AFTER_BUILD  TRUE
)

# RKMix's own sources (also used by RKBoardTests and the snapshot tool).
set(RKMIX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixPanels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixGroups.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixUndo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixLock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixGang.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixQr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixWebServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixRemote.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixConsole.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixOsc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixRemotePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixComponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixMeasure.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixMonitorEq.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixMonitorEqSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixMonitorEqView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixSendEqView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixRecorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixRecorderSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixStrip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixSendEq.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixBusDsp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixInserts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixScenes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixAssigns.cpp
)
set(RKMIX_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/plugins/RKCompress     # the compressor's transfer curve display
)

# RKMix's factory presets (presets/*.json, whole consoles), registered by
# name, with the pedal host (every insert and its presets).
add_library(rkmix_factory_presets INTERFACE)
file(GLOB mixPresets CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.json")
list(SORT mixPresets)
set(PLUGIN RKMix)
if(mixPresets)
    juce_add_binary_data(RKMix_FactoryPresets
        HEADER_NAME RKMixFactoryPresetData.h
        NAMESPACE   RKMixFactoryPresetData
        SOURCES     ${mixPresets})
    target_link_libraries(rkmix_factory_presets INTERFACE RKMix_FactoryPresets)
    configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetList.cpp.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/RKMixFactoryPresetList.cpp" @ONLY)
else()
    configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetListEmpty.cpp.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/RKMixFactoryPresetList.cpp" @ONLY)
endif()
target_sources(rkmix_factory_presets INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/RKMixFactoryPresetList.cpp")
target_link_libraries(rkmix_factory_presets INTERFACE rkfx_pedal_host)
# Names by OSC (MixOsc): every target with RKMix's sources (the tests, the tools) gets it too.
target_link_libraries(rkmix_factory_presets INTERFACE juce::juce_osc)

# The remote's web pages (remote/: the monitor page, the console app), served
# by MixRemote; looked up by file name (MixRemotePage::find).
file(GLOB remoteFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/remote/*.html"
                                        "${CMAKE_CURRENT_SOURCE_DIR}/remote/*.js"
                                        "${CMAKE_CURRENT_SOURCE_DIR}/remote/*.css")
list(SORT remoteFiles)
juce_add_binary_data(RKMix_RemoteFiles
    HEADER_NAME RKMixRemoteFiles.h
    NAMESPACE   RKMixRemoteFiles
    SOURCES     ${remoteFiles})
target_link_libraries(rkmix_factory_presets INTERFACE RKMix_RemoteFiles)

set(RKMIX_SOURCES ${RKMIX_SOURCES} PARENT_SCOPE)
set(RKMIX_INCLUDE_DIRS ${RKMIX_INCLUDE_DIRS} PARENT_SCOPE)

target_sources(RKMix PRIVATE ${RKMIX_SOURCES})
target_include_directories(RKMix PRIVATE ${RKMIX_INCLUDE_DIRS})

target_compile_definitions(RKMix
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(RKMix
    PRIVATE
        rkmix_factory_presets
        rkfx_core
        juce::juce_audio_processors
        juce::juce_audio_utils          # Standalone wrapper (device settings)
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
