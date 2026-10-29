#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "PolyPitchEngine.h"

#include <atomic>

/** RKPolyPitch: a polyphonic pitch shifter, whammy style (Whammy, Harmony,
    Detune). Whammy mode is RKFX's one latency exception: the shifted sound
    trails the input by a few milliseconds (not reported to the host). */
class RKPolyPitchProcessor : public rkfx::ProcessorBase
{
public:
    RKPolyPitchProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    PolyPitchEngine::Settings currentSettings() const;

    /** For the display. */
    std::atomic<float> shownShift { 0.0f }, shownPosition { 0.0f };

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *mode, *range, *heel, *toe, *pedal, *glide, *momentary, *rise, *fall, *fine, *detune,
                               *voiceLevel, *width, *tone, *lowLatency;
    PolyPitchEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKPolyPitchProcessor)
};
