# DSP and format tests in isolation. Links ONLY juce_core and
# juce_audio_basics, which structurally keeps the DSP and preset code
# decoupled from the plugin and GUI layers.
juce_add_console_app(RKFXTests PRODUCT_NAME "RKFXTests")

target_sources(RKFXTests PRIVATE
    TestMain.cpp
    SafetyTests.cpp
    OutputStageTests.cpp
    PresetFormatTests.cpp
    PresetLibraryTests.cpp
)

target_compile_definitions(RKFXTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)

target_link_libraries(RKFXTests
    PRIVATE
        rkfx_core_dsp
        juce::juce_core
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_test(NAME RKFXTests COMMAND RKFXTests)
