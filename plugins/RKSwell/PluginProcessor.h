#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "SwellEngine.h"

/** RKSwell: auto swell that fades in each note (bowed, violin-like). */
class RKSwellProcessor : public rkfx::ProcessorBase
{
public:
    RKSwellProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    SwellEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *threshold, *time, *curve, *retrigger, *link;
    SwellEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKSwellProcessor)
};
