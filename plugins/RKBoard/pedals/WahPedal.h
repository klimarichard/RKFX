#pragma once

#include "plugin/EditorBase.h"
#include "plugin/ProcessorBase.h"

#include "WahEngine.h"

/** Wah pedal (RKBoard only): for an expression pedal through an Assign
    parameter. With auto engage, moving the pedal switches it on and resting
    at the heel switches it off, so no footswitch is needed.
    Parameter IDs are permanent: never rename, reuse or reorder; only append. */
class RKWahProcessor : public rkfx::ProcessorBase
{
public:
    RKWahProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    WahEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *position, *character, *freqLow, *freqHigh, *resonance, *autoEngage;
    WahEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKWahProcessor)
};

class RKWahEditor : public rkfx::EditorBase
{
public:
    explicit RKWahEditor (RKWahProcessor&);

private:
    std::unique_ptr<rkfx::Knob> position, freqLow, freqHigh, resonance;
    std::unique_ptr<rkfx::SegmentedSwitch> character;
    std::unique_ptr<rkfx::ToggleSwitch> autoEngage;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKWahEditor)
};
