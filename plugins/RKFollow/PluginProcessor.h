#pragma once

#include "plugin/ProcessorBase.h"

#include "FollowEngine.h"
#include "Parameters.h"

#include <atomic>

/** RKFollow: a single line (violin, voice...) to MIDI notes, pitch bend and
    an expression controller. The audio passes through untouched. */
class RKFollowProcessor : public rkfx::ProcessorBase
{
public:
    RKFollowProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    FollowEngine::Settings currentSettings() const;

    /** For the display (message thread): the level is the loudest since the last call. */
    FollowEngine::Meter takeMeter() noexcept;

    /** Any note, the expression controller or pressure, pitch bend and its range
        (RPN) on its channel; in MPE mode on every channel. */
    rkfx::MidiFootprint getMidiOutputFootprint() const override;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;
    void releaseMidi (juce::MidiBuffer&) override;

private:
    juce::RangedAudioParameter *notes, *range, *threshold, *transpose, *velMin, *velMax, *retrigger, *bend, *bendRange,
                               *glide, *channel, *expr, *exprCc, *exprFloor, *exprCeiling, *exprMin, *exprMax, *exprCurve,
                               *exprResponse;

    FollowEngine engine;
    juce::MidiBuffer scratch;

    std::atomic<int> meterNote { -1 }, meterVelocity { 0 }, meterExpression { -1 };
    std::atomic<float> meterBend { 0.0f }, meterLevel { rkfx::Dynamics::kSilenceDb };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFollowProcessor)
};
