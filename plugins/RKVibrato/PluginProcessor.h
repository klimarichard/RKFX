#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "VibratoEngine.h"

/** RKVibrato: pure pitch vibrato with tempo sync, stereo phase and rise. */
class RKVibratoProcessor : public rkfx::ProcessorBase
{
public:
    RKVibratoProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    VibratoEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *rate, *depth, *shape, *sync, *division, *phase, *rise;
    VibratoEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVibratoProcessor)
};
