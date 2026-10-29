# Step 0 of RKRemap (docs/design/RKRemap.md): two throwaway test plugins with
# 64 inputs and 64 outputs, identity routing and a light per input, to check
# how Gig Performer shows and saves them. Built only with -DRKFX_BUILD_REMAP_TEST=ON.

foreach(variant IN ITEMS 64 32x2)
    if(variant STREQUAL "64")
        set(code Rt64)
        set(oneBus 1)
    else()
        set(code Rt32)
        set(oneBus 0)
    endif()

    set(target RKRemapTest${variant})
    juce_add_plugin(${target}
        COMPANY_NAME             "RKMusic"
        PLUGIN_MANUFACTURER_CODE RKMu
        PLUGIN_CODE              ${code}
        BUNDLE_ID                com.rkmusic.rkremaptest${variant}
        PRODUCT_NAME             "${target}"
        FORMATS                  AU VST3
        IS_SYNTH                 FALSE
        NEEDS_MIDI_INPUT         FALSE
        NEEDS_MIDI_OUTPUT        FALSE
        AU_MAIN_TYPE             kAudioUnitType_Effect
        VST3_CATEGORIES          Fx
        COPY_PLUGIN_AFTER_BUILD  TRUE
    )
    target_sources(${target} PRIVATE RemapTest.cpp)
    target_compile_definitions(${target} PRIVATE
        REMAP_TEST_ONE_BUS=${oneBus}
        REMAP_TEST_NAME="${target}"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${target}
        PRIVATE juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endforeach()
