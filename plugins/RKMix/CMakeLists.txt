# RKMix: the mixing console. Its own sources are listed once (RKMIX_SOURCES)
# for the plugin, the state tests and the snapshot tool.
set(RKMIX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixComponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixStrip.cpp
)
set(RKMIX_SOURCES ${RKMIX_SOURCES} PARENT_SCOPE)

# The other plugins' code RKMix uses: RKCompress's transfer curve display
# (RKBoardTests has it already).
set(RKMIX_SHARED_SOURCES
    ${PROJECT_SOURCE_DIR}/plugins/RKCompress/TransferCurve.cpp
)
set(RKMIX_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/plugins/RKCompress
)
set(RKMIX_SHARED_SOURCES ${RKMIX_SHARED_SOURCES} PARENT_SCOPE)
set(RKMIX_INCLUDE_DIRS ${RKMIX_INCLUDE_DIRS} PARENT_SCOPE)

rkfx_add_plugin(RKMix
    CODE      Mix1
    BUNDLE_ID com.rkmusic.rkmix
    MIDI_INPUT
    SOURCES   ${RKMIX_SOURCES} ${RKMIX_SHARED_SOURCES}
)
target_include_directories(RKMix PRIVATE ${RKMIX_INCLUDE_DIRS})
