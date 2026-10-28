#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "RingModEngine.h"

/** RKRingMod: ring modulator with a free, pitch-tracked or tempo-synced carrier. */
class RKRingModProcessor : public rkfx::ProcessorBase
{
public:
    RKRingModProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    RingModEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *mode, *wave, *frequency, *interval, *division, *range, *lfoDepth, *lfoRate;
    RingModEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKRingModProcessor)
};
