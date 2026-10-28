#pragma once

#include "plugin/ProcessorBase.h"

#include "DeEssEngine.h"
#include "Parameters.h"

#include <atomic>

/** RKDeEss: split-band or wideband de-esser with relative or absolute detection. */
class RKDeEssProcessor : public rkfx::ProcessorBase
{
public:
    RKDeEssProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    DeEssEngine::Settings currentSettings() const;

    float takeGainReductionDb() noexcept { return meterReduction.exchange (0.0f); }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *freq, *detect, *threshold, *thresholdRel, *range, *mode, *listen, *link;
    DeEssEngine engine;
    std::atomic<float> meterReduction { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKDeEssProcessor)
};
