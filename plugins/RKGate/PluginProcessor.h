#pragma once

#include "plugin/ProcessorBase.h"

#include "GateEngine.h"
#include "Parameters.h"

#include <atomic>

/** RKGate: gate and expander with external sidechain, key filter and gain
    reduction meter. */
class RKGateProcessor : public rkfx::ProcessorBase
{
public:
    RKGateProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    GateEngine::Settings currentSettings() const;

    /** Most gain reduction since the last call (message thread, for the meter). */
    float takeGainReductionDb() noexcept { return meterReduction.exchange (0.0f); }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *mode, *threshold, *ratio, *range, *attack, *hold, *release, *hysteresis,
                               *scSource, *keyHpf, *keyLpf, *keyListen, *link;

    GateEngine engine;
    std::atomic<float> meterReduction { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKGateProcessor)
};
