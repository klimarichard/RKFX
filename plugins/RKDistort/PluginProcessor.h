#pragma once

#include "plugin/ProcessorBase.h"

#include "DistortEngine.h"
#include "Parameters.h"

/** RKDistort: overdrive, distortion and fuzz with tight, mid, tone, a cab
    filter and selectable oversampling (latency reported to the host). */
class RKDistortProcessor : public rkfx::ProcessorBase
{
public:
    RKDistortProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    DistortEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *type, *gain, *tight, *mid, *tone, *cab, *autoGain, *oversampling;
    DistortEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKDistortProcessor)
};
