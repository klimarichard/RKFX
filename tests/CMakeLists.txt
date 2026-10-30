# Every plugin's GUI-free DSP engine, shared by the tests and the benchmark.
# A new plugin adds one line here.
set(RKFX_ENGINE_PLUGINS
    RKFreqShift/FreqShiftEngine
    RKFilter/FilterEngine
    RKParaEQ/ParaEQEngine
    RKGraphEQ/GraphEQEngine
    RKCompress/CompressorEngine
    RKGate/GateEngine
    RKTremolo/TremoloEngine
    RKChorus/ChorusEngine
    RKDouble/DoubleEngine
    RKFlange/FlangeEngine
    RKPhase/PhaseEngine
    RKDelay/DelayEngine
    RKSaturate/SaturateEngine
    RKDistort/DistortEngine
    RKVerb/ReverbEngine
    RKLimit/LimitEngine
    RKTransient/TransientEngine
    RKDeEss/DeEssEngine
    RKSwell/SwellEngine
    RKRotary/RotaryEngine
    RKVibrato/VibratoEngine
    RKVocode/VocodeEngine
    RKUtility/UtilityEngine
    RKOctave/OctaveEngine
    RKAcoustic/AcousticEngine
    RKPreamp/PreampEngine
    RKSynth/SynthEngine
    RKRingMod/RingModEngine
    RKLoop/LoopEngine
    RKVoice/VoiceEngine
    RKMultiComp/MultiCompEngine
    RKRide/RideEngine
    RKFollow/FollowEngine
    RKTrigger/TriggerEngine
    RKHold/HoldEngine
    RKEcho/EchoEngine
    RKPolyPitch/PolyPitchEngine
    RKRemap/RemapEngine
    RKMix/MixEngine
    RKMix/MixStrip
    RKHarmonize/HarmonizeEngine
    RKArpeggiate/ArpEngine
    RKStrum/StrumEngine
    RKSustain/SustainEngine
    RKConduct/ConductEngine
)

set(RKFX_ENGINE_SOURCES "")
set(RKFX_ENGINE_DIRS "")
foreach(engine IN LISTS RKFX_ENGINE_PLUGINS)
    get_filename_component(engineDir "${engine}" DIRECTORY)
    list(APPEND RKFX_ENGINE_SOURCES "${PROJECT_SOURCE_DIR}/plugins/${engine}.cpp")
    list(APPEND RKFX_ENGINE_DIRS "${PROJECT_SOURCE_DIR}/plugins/${engineDir}")
endforeach()

# DSP and format tests in isolation. Links ONLY juce_core and
# juce_audio_basics, which structurally keeps the DSP and preset code
# decoupled from the plugin and GUI layers.
juce_add_console_app(RKFXTests PRODUCT_NAME "RKFXTests")

target_sources(RKFXTests PRIVATE
    TestMain.cpp
    SafetyTests.cpp
    LinkwitzRileyTests.cpp
    OutputStageTests.cpp
    PresetFormatTests.cpp
    PresetLibraryTests.cpp
    FreqShiftTests.cpp
    FilterTests.cpp
    ParaEQTests.cpp
    ParaEQDynamicTests.cpp
    GraphEQTests.cpp
    CompressTests.cpp
    GateTests.cpp
    TremoloTests.cpp
    ChorusTests.cpp
    DoubleTests.cpp
    FlangeTests.cpp
    PhaseTests.cpp
    DelayTests.cpp
    SaturateTests.cpp
    DistortTests.cpp
    VerbTests.cpp
    LimitTests.cpp
    TransientTests.cpp
    DeEssTests.cpp
    SwellTests.cpp
    RotaryTests.cpp
    VibratoTests.cpp
    VocodeTests.cpp
    UtilityTests.cpp
    OctaveTests.cpp
    AcousticTests.cpp
    PreampTests.cpp
    SynthTests.cpp
    VoiceTests.cpp
    RingModTests.cpp
    LoopTests.cpp
    HarmonizeTests.cpp
    ArpTests.cpp
    StrumTests.cpp
    SustainTests.cpp
    ConductTests.cpp
    MultiCompTests.cpp
    RideTests.cpp
    ShimmerTests.cpp
    FollowTests.cpp
    TriggerTests.cpp
    HoldTests.cpp
    EchoTests.cpp
    PolyPitchTests.cpp
    RemapTests.cpp
    MixTests.cpp
    ${RKFX_ENGINE_SOURCES}
)

target_include_directories(RKFXTests PRIVATE ${RKFX_ENGINE_DIRS})

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
    ${RKFX_ENGINE_SOURCES}
)
target_include_directories(RKFXBench PRIVATE ${RKFX_ENGINE_DIRS})
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

# Factory preset check: every plugins/<Name>/presets/*.json against that
# plugin's real parameters. Needs the parameter classes, so it links
# juce_audio_processors (the DSP tests above deliberately don't).
juce_add_console_app(RKFXPresetTests PRODUCT_NAME "RKFXPresetTests")
target_sources(RKFXPresetTests PRIVATE
    PresetTests.cpp
    ${PROJECT_SOURCE_DIR}/core/plugin/ParamBuilder.cpp
    ${PROJECT_SOURCE_DIR}/plugins/RKGraphEQ/GraphEQEngine.cpp
)
target_compile_definitions(RKFXPresetTests PRIVATE
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    RKFX_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(RKFXPresetTests
    PRIVATE
        rkfx_core_dsp
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
add_test(NAME RKFXPresetTests COMMAND RKFXPresetTests)

# RKBoard and RKMix: the board and the console with every plugin as a pedal
# (through the pedal host, like the RKBoard and RKMix targets themselves).
juce_add_console_app(RKBoardTests PRODUCT_NAME "RKBoardTests")
target_sources(RKBoardTests PRIVATE
    BoardTests.cpp
    RemapStateTests.cpp
    MixStateTests.cpp
    ${RKMIX_SOURCES}
    ${RKBOARD_SOURCES}
)
target_include_directories(RKBoardTests PRIVATE ${PROJECT_SOURCE_DIR}/plugins/RKBoard ${RKMIX_INCLUDE_DIRS})
target_compile_definitions(RKBoardTests PRIVATE
    RKMIX_NO_ENTRY_POINT=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    RKFX_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(RKBoardTests
    PRIVATE
        rkboard_factory_presets
        rkmix_factory_presets
        rkfx_core
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
add_test(NAME RKBoardTests COMMAND RKBoardTests)
