#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "ReverbEngine.h"

/** RKVerb: algorithmic reverb with room, hall and plate types and freeze. */
class RKVerbProcessor : public rkfx::ProcessorBase
{
public:
    RKVerbProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    ReverbEngine::Settings currentSettings (double bpm) const;

    /** The longest decay plus room for the pre-delay. */
    double getTailLengthSeconds() const override { return 21.0; }

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *type, *decay, *size, *predelay, *predelaySync, *predelayDivision,
                               *damping, *lowCut, *highCut, *mod, *width, *freeze;
    ReverbEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVerbProcessor)
};
