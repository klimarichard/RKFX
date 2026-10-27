#pragma once

#include "plugin/ProcessorBase.h"

#include "DoubleEngine.h"
#include "Parameters.h"

/** RKDouble: artificial double tracking. */
class RKDoubleProcessor : public rkfx::ProcessorBase
{
public:
    RKDoubleProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    DoubleEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *delay, *drift, *driftSpeed, *tone, *spread, *level;
    DoubleEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKDoubleProcessor)
};
