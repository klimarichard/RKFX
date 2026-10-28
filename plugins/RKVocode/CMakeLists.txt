rkfx_add_plugin(RKVocode
    CODE      Voc1
    BUNDLE_ID com.rkmusic.rkvocode
    MIDI_INPUT
    SOURCES
        PluginProcessor.cpp
        PluginEditor.cpp
        VocodeEngine.cpp
)
