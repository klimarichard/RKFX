#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "SynthEngine.h"

/** RKSynth: a monophonic synth played by the instrument (bass, guitar, violin). */
class RKSynthProcessor : public rkfx::ProcessorBase
{
public:
    RKSynthProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    SynthEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *range, *wave, *octave, *sub, *glide, *cutoff, *resonance, *envAmount,
                               *decay, *dynamics, *threshold;
    SynthEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKSynthProcessor)
};
