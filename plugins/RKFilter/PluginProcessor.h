#pragma once

#include "plugin/ProcessorBase.h"

#include "FilterEngine.h"
#include "Parameters.h"

/** RKFilter: multimode filter with envelope (auto-wah) and LFO modulation. */
class RKFilterProcessor : public rkfx::ProcessorBase
{
public:
    RKFilterProcessor();

    juce::AudioProcessorEditor* createEditor() override;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *mode, *slope, *cutoff, *resonance,
                               *envAmount, *envSens, *envAttack, *envRelease,
                               *lfoDepth, *lfoShape, *lfoRate, *lfoSync, *lfoDivision, *lfoPhase;

    FilterEngine engine;

    void updateEngine();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFilterProcessor)
};
