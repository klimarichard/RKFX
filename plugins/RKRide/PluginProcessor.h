#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "RideEngine.h"

#include <atomic>

/** RKRide: an automatic level rider, with an optional sidechain so the
    target follows the music. */
class RKRideProcessor : public rkfx::ProcessorBase
{
public:
    RKRideProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    RideEngine::Settings currentSettings() const;

    /** The meter since the last call (message thread, for the graph and the
        fader). @p fresh is false when no audio has been processed since. */
    RideEngine::Meter takeMeter (bool& fresh) noexcept;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *target, *maxBoost, *maxCut, *speed, *floor, *returnAfter, *mode, *above;

    RideEngine engine;

    std::atomic<float> meterLevel { rkfx::Dynamics::kSilenceDb }, meterTarget { -18.0f }, meterGain { 0.0f };
    std::atomic<bool> meterHolding { false }, meterFresh { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKRideProcessor)
};
