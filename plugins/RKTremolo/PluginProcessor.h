#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "TremoloEngine.h"

/** RKTremolo: tremolo and auto-panner with a harmonic (brownface) mode. */
class RKTremoloProcessor : public rkfx::ProcessorBase
{
public:
    RKTremoloProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    TremoloEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *mode, *depth, *rate, *sync, *division, *shape, *smooth, *phase, *xover;
    TremoloEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKTremoloProcessor)
};
