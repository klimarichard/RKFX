#pragma once

#include "plugin/ProcessorBase.h"

#include "FreqShiftEngine.h"
#include "Parameters.h"

/** RKFreqShift: four independent Bode-style frequency shifts mixed to one
    stereo output. */
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
    struct ShiftParams
    {
        juce::RangedAudioParameter *on, *freq, *fine, *range, *updown, *feedback, *level;
    };

    std::array<ShiftParams, FreqShiftParams::kNumShifts> shiftParams {};
    juce::RangedAudioParameter *spread = nullptr, *lowCut = nullptr, *highCut = nullptr;

    FreqShiftEngine engine;

    void updateEngine();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFreqShiftProcessor)
};
