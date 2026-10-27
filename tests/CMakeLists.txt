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
    FreqShiftTests.cpp
    FilterTests.cpp
    ParaEQTests.cpp
    GraphEQTests.cpp
    CompressTests.cpp
    GateTests.cpp
    TremoloTests.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKFreqShift/FreqShiftEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKFilter/FilterEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKParaEQ/ParaEQEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKGraphEQ/GraphEQEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKCompress/CompressorEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKGate/GateEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKTremolo/TremoloEngine.cpp
)

target_include_directories(RKFXTests PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/RKFreqShift
    ${PROJECT_SOURCE_DIR}/plugins/RKFilter
    ${PROJECT_SOURCE_DIR}/plugins/RKParaEQ
    ${PROJECT_SOURCE_DIR}/plugins/RKGraphEQ
    ${PROJECT_SOURCE_DIR}/plugins/RKCompress
    ${PROJECT_SOURCE_DIR}/plugins/RKGate
    ${PROJECT_SOURCE_DIR}/plugins/RKTremolo)

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

# CPU benchmarks (48 kHz, 64-sample blocks). Not part of ctest: run
# RKFXBench directly, or with --record <csv> to log results per build.
juce_add_console_app(RKFXBench PRODUCT_NAME "RKFXBench")
target_sources(RKFXBench PRIVATE
    Benchmark.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKFreqShift/FreqShiftEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKFilter/FilterEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKParaEQ/ParaEQEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKGraphEQ/GraphEQEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKCompress/CompressorEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKGate/GateEngine.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKTremolo/TremoloEngine.cpp
)
target_include_directories(RKFXBench PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/RKFreqShift
    ${PROJECT_SOURCE_DIR}/plugins/RKFilter
    ${PROJECT_SOURCE_DIR}/plugins/RKParaEQ
    ${PROJECT_SOURCE_DIR}/plugins/RKGraphEQ
    ${PROJECT_SOURCE_DIR}/plugins/RKCompress
    ${PROJECT_SOURCE_DIR}/plugins/RKGate
    ${PROJECT_SOURCE_DIR}/plugins/RKTremolo)
target_compile_definitions(RKFXBench PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(RKFXBench
    PRIVATE
        rkfx_core_dsp
        juce::juce_core
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
