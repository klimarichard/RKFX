# Optional: offscreen editor snapshots for checking the GUI without a host.
# Not built by default; build e.g. `cmake --build build --target RKFXSnapshots`.
# Each tool compiles the plugin's own sources as a plain app.
add_custom_target(RKFXSnapshots)

foreach(plugin IN LISTS RKFX_PLUGINS)
    set(tool RKFXSnapshot_${plugin})
    juce_add_console_app(${tool} PRODUCT_NAME "${tool}")
    set_target_properties(${tool} PROPERTIES EXCLUDE_FROM_ALL TRUE)

    file(GLOB pluginSources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/plugins/${plugin}/*.cpp")
    target_sources(${tool} PRIVATE Snapshot.cpp ${pluginSources} "${CMAKE_BINARY_DIR}/plugins/${plugin}/FactoryPresetList.cpp")
    target_include_directories(${tool} PRIVATE "${PROJECT_SOURCE_DIR}/plugins/${plugin}")
    if(TARGET ${plugin}_FactoryPresets)
        target_link_libraries(${tool} PRIVATE ${plugin}_FactoryPresets)
    endif()

    target_compile_definitions(${tool} PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${tool} PRIVATE rkfx_core juce::juce_audio_processors juce::juce_gui_basics
                          PUBLIC juce::juce_recommended_config_flags)
    add_dependencies(RKFXSnapshots ${tool})
endforeach()
