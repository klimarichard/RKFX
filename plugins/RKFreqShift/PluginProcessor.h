#pragma once

#include "plugin/ProcessorBase.h"

/** RKFreqShift: four-voice Bode-style frequency shifter.

    Foundation shell: standard controls only, the signal passes through
    unchanged. The shifter DSP and its parameters come in the next step. */
class RKFreqShiftProcessor : public rkfx::ProcessorBase
{
public:
    RKFreqShiftProcessor();

    juce::AudioProcessorEditor* createEditor() override;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFreqShiftProcessor)
};
