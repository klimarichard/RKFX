#pragma once

#include "plugin/ProcessorBase.h"

#include "FlangeEngine.h"
#include "Parameters.h"

/** RKFlange: classic flanger with positive and negative feedback. */
class RKFlangeProcessor : public rkfx::ProcessorBase
{
public:
    RKFlangeProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    FlangeEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *rate, *depth, *manual, *feedback, *sync, *division, *phase;
    FlangeEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFlangeProcessor)
};
