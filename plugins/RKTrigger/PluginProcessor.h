#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "TriggerEngine.h"

#include <array>
#include <atomic>

/** RKTrigger: hits (piezo, drum mic, a knock) to MIDI notes, one trigger or
    two. The audio passes through, or is muted. */
class RKTriggerProcessor : public rkfx::ProcessorBase
{
public:
    RKTriggerProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    TriggerEngine::Settings currentSettings() const;

    /** For the display (message thread): the level is the loudest since the last call. */
    TriggerEngine::Meter takeMeter (int trigger) noexcept;

    /** The trigger whose controls the editor shows (kept while loaded; not a parameter). */
    std::atomic<int> selectedTrigger { 0 };

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;
    void releaseMidi (juce::MidiBuffer&) override;

private:
    struct TriggerParamSet
    {
        juce::RangedAudioParameter *threshold, *hpf, *lpf, *scan, *mask, *retrigger, *dynamics, *curve,
                                   *velMin, *velMax, *note, *channel, *length;
    };
    std::array<TriggerParamSet, TriggerParams::kTriggers> triggerParams;
    juce::RangedAudioParameter *mode, *crosstalk, *audio;

    TriggerEngine engine;
    juce::MidiBuffer scratch;

    struct AtomicMeter
    {
        std::atomic<float> levelDb { rkfx::Dynamics::kSilenceDb };
        std::atomic<int> hits { 0 }, velocity { 0 };
    };
    std::array<AtomicMeter, TriggerParams::kTriggers> meters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKTriggerProcessor)
};
