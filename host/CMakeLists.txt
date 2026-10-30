# The RKFX pedal host: every RKFX audio plugin built in as a pedal (their own
# entry points left out with RKFX_BOARD), the pedals that are no plugins of
# their own (Volume, Wah), and the registry RKBoard and RKMix create them
# from, with every pedal's factory presets.
#
# An INTERFACE library like the core: each consumer (RKBoard, RKMix, their
# tests and snapshot tools) compiles the sources itself.

add_library(rkfx_pedal_host INTERFACE)

set(hostSources
    ${CMAKE_CURRENT_SOURCE_DIR}/PedalRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pedals/VolumePedal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pedals/WahPedal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pedals/WahEngine.cpp
)

# The host's own pedals' factory presets (pedals/presets/<Name>/*.json).
foreach(pedal RKVolume RKWah)
    file(GLOB pedalPresets CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pedals/presets/${pedal}/*.json")
    list(SORT pedalPresets)
    juce_add_binary_data(${pedal}_FactoryPresets
        HEADER_NAME ${pedal}FactoryPresetData.h
        NAMESPACE   ${pedal}FactoryPresetData
        SOURCES     ${pedalPresets})
    target_link_libraries(rkfx_pedal_host INTERFACE ${pedal}_FactoryPresets)

    set(PLUGIN ${pedal})
    configure_file("${RKFX_CORE_DIR}/plugin/FactoryPresetList.cpp.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/${pedal}FactoryPresetList.cpp" @ONLY)
    list(APPEND hostSources "${CMAKE_CURRENT_BINARY_DIR}/${pedal}FactoryPresetList.cpp")
endforeach()

# Every plugin's own sources, factory preset list and preset data (and the
# audio effects that send MIDI, for RKMix's inserts).
foreach(plugin IN LISTS RKFX_PLUGINS RKFX_HOST_MIDI_PLUGINS)
    file(GLOB pluginSources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/plugins/${plugin}/*.cpp")
    list(APPEND hostSources ${pluginSources} "${CMAKE_BINARY_DIR}/plugins/${plugin}/FactoryPresetList.cpp")
    if(TARGET ${plugin}_FactoryPresets)
        target_link_libraries(rkfx_pedal_host INTERFACE ${plugin}_FactoryPresets)
    endif()
endforeach()

target_sources(rkfx_pedal_host INTERFACE ${hostSources})
target_include_directories(rkfx_pedal_host INTERFACE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(rkfx_pedal_host INTERFACE RKFX_BOARD=1)
target_link_libraries(rkfx_pedal_host INTERFACE rkfx_core)
