rkfx_add_plugin(RKTrigger
    CODE      Trg1
    BUNDLE_ID com.rkmusic.rktrigger
    MIDI_OUTPUT
    SOURCES
        PluginProcessor.cpp
        PluginEditor.cpp
        TriggerEngine.cpp
)
