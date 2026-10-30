# Optional: offscreen editor snapshots for checking the GUI without a host.
# Not built by default; build e.g. `cmake --build build --target RKFXSnapshots`.
# Each tool compiles the plugin's own sources as a plain app.
add_custom_target(RKFXSnapshots)

foreach(plugin IN LISTS RKFX_PLUGINS RKFX_MIDI_PLUGINS)
    set(tool RKFXSnapshot_${plugin})
    juce_add_console_app(${tool} PRODUCT_NAME "${tool}")
    set_target_properties(${tool} PROPERTIES EXCLUDE_FROM_ALL TRUE)

    file(GLOB pluginSources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/plugins/${plugin}/*.cpp")
    target_sources(${tool} PRIVATE Snapshot.cpp ${pluginSources} "${CMAKE_BINARY_DIR}/plugins/${plugin}/FactoryPresetList.cpp")
    target_include_directories(${tool} PRIVATE "${PROJECT_SOURCE_DIR}/plugins/${plugin}")
    if(TARGET ${plugin}_FactoryPresets)
        target_link_libraries(${tool} PRIVATE ${plugin}_FactoryPresets)
    endif()

    target_compile_definitions(${tool} PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_DISPLAY_SPLASH_SCREEN=0
                                           RKFX_FACTORY_PRESET_LIST=factoryPresetList_${plugin})
    target_link_libraries(${tool} PRIVATE rkfx_core juce::juce_audio_processors juce::juce_gui_basics
                          PUBLIC juce::juce_recommended_config_flags)
    add_dependencies(RKFXSnapshots ${tool})
endforeach()

# RKBoard and RKMix: every plugin's sources through the pedal host.
juce_add_console_app(RKFXSnapshot_RKBoard PRODUCT_NAME "RKFXSnapshot_RKBoard")
set_target_properties(RKFXSnapshot_RKBoard PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(RKFXSnapshot_RKBoard PRIVATE Snapshot.cpp ${RKBOARD_SOURCES})
target_include_directories(RKFXSnapshot_RKBoard PRIVATE ${PROJECT_SOURCE_DIR}/plugins/RKBoard)
target_compile_definitions(RKFXSnapshot_RKBoard PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(RKFXSnapshot_RKBoard PRIVATE rkboard_factory_presets rkfx_core juce::juce_audio_processors juce::juce_gui_basics
                      PUBLIC juce::juce_recommended_config_flags)
add_dependencies(RKFXSnapshots RKFXSnapshot_RKBoard)

juce_add_console_app(RKFXSnapshot_RKMix PRODUCT_NAME "RKFXSnapshot_RKMix")
set_target_properties(RKFXSnapshot_RKMix PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(RKFXSnapshot_RKMix PRIVATE Snapshot.cpp ${RKMIX_SOURCES})
target_include_directories(RKFXSnapshot_RKMix PRIVATE ${RKMIX_INCLUDE_DIRS})
target_compile_definitions(RKFXSnapshot_RKMix PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(RKFXSnapshot_RKMix PRIVATE rkmix_factory_presets rkfx_core juce::juce_audio_processors juce::juce_gui_basics
                      PUBLIC juce::juce_recommended_config_flags)
add_dependencies(RKFXSnapshots RKFXSnapshot_RKMix)

juce_add_console_app(RKFXSnapshot_RKPlay PRODUCT_NAME "RKFXSnapshot_RKPlay")
set_target_properties(RKFXSnapshot_RKPlay PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(RKFXSnapshot_RKPlay PRIVATE Snapshot.cpp ${RKPLAY_SOURCES})
target_include_directories(RKFXSnapshot_RKPlay PRIVATE ${RKPLAY_INCLUDE_DIRS})
target_compile_definitions(RKFXSnapshot_RKPlay PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(RKFXSnapshot_RKPlay PRIVATE rkplay_factory_presets rkfx_core juce::juce_audio_formats
                      juce::juce_audio_processors juce::juce_gui_basics
                      PUBLIC juce::juce_recommended_config_flags)
add_dependencies(RKFXSnapshots RKFXSnapshot_RKPlay)

# The RKMusic plugin icons (tools/icons): renders each plugin's icon_big.png
# and icon_small.png. Not built by default; see tools/icons/RenderIcons.cpp.
juce_add_console_app(RKIconRender PRODUCT_NAME "RKIconRender")
set_target_properties(RKIconRender PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(RKIconRender PRIVATE icons/RenderIcons.cpp)
target_compile_definitions(RKIconRender PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(RKIconRender PRIVATE juce::juce_graphics juce::juce_events
                      PUBLIC juce::juce_recommended_config_flags)
