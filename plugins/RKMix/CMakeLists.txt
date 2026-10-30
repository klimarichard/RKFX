# RKMix: the mixing console. Its own sources are listed once (RKMIX_SOURCES)
# for the plugin, the state tests and the snapshot tool.
set(RKMIX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixComponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixEngine.cpp
)
set(RKMIX_SOURCES ${RKMIX_SOURCES} PARENT_SCOPE)

rkfx_add_plugin(RKMix
    CODE      Mix1
    BUNDLE_ID com.rkmusic.rkmix
    MIDI_INPUT
    SOURCES   ${RKMIX_SOURCES}
)
