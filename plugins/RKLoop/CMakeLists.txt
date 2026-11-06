rkfx_add_plugin(RKLoop
    CODE      Lop1
    BUNDLE_ID com.rkmusic.rkloop
    SOURCES
        PluginProcessor.cpp
        PluginEditor.cpp
        LoopEngine.cpp
        LoopExport.cpp
)

# Export writes WAV files.
target_link_libraries(RKLoop PRIVATE juce::juce_audio_formats)
