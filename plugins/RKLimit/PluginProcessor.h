#pragma once

#include "plugin/ProcessorBase.h"

#include "LimitEngine.h"
#include "Parameters.h"

#include <atomic>

/** RKLimit: zero-latency peak limiter for protecting PA and in-ear mixes. */
class RKLimitProcessor : public rkfx::ProcessorBase
{
public:
    RKLimitProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    LimitEngine::Settings currentSettings() const;

    /** Most gain reduction since the last call (message thread, for the meter). */
    float takeGainReductionDb() noexcept { return meterReduction.exchange (0.0f); }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *input, *ceiling, *release, *releaseAuto, *link, *oversampling;
    LimitEngine engine;
    std::atomic<float> meterReduction { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKLimitProcessor)
};
