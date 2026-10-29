#pragma once

#include "plugin/ProcessorBase.h"

#include "DelayEngine.h"
#include "Parameters.h"

/** RKDelay: stereo delay with digital, analog and tape characters. */
class RKDelayProcessor : public rkfx::ProcessorBase
{
public:
    RKDelayProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    DelayEngine::Settings currentSettings() const;

    /** Repeats can ring for a long time at high feedback. */
    double getTailLengthSeconds() const override { return 10.0; }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *mode, *time, *sync, *division, *feedback, *stereo, *offset,
                               *lowCut, *highCut, *modDepth, *modRate, *duck,
                               *shimmer, *shimmerInterval;
    DelayEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKDelayProcessor)
};
