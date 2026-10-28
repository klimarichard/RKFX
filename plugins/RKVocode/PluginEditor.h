#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

class RKVocodeProcessor;

class RKVocodeEditor : public rkfx::EditorBase
{
public:
    explicit RKVocodeEditor (RKVocodeProcessor&);

private:
    RKVocodeProcessor& vocoder;
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::SegmentedSwitch> bands, carrier, wave, mode;
    std::unique_ptr<rkfx::Knob> freqLow, freqHigh, formant, attack, release, unvoiced, modMix, note, voices, glide;

    juce::Rectangle<int> vocoderArea() const;
    juce::Rectangle<int> carrierArea() const;

    void onParameterChange();
    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVocodeEditor)
};
