#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "SaturateEngine.h"

/** RKSaturate: tape, tube and transformer saturation with auto gain and
    selectable oversampling (the only RKFX plugin besides RKDistort and
    RKLimit that can add latency, reported to the host). */
class RKSaturateProcessor : public rkfx::ProcessorBase
{
public:
    RKSaturateProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    SaturateEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *drive, *character, *tone, *autoGain, *oversampling;
    SaturateEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKSaturateProcessor)
};
