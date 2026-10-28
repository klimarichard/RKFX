#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "PreampEngine.h"

/** RKPreamp: a guitar amp's preamp ("amp in a box"), four voicings. */
class RKPreampProcessor : public rkfx::ProcessorBase
{
public:
    RKPreampProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    PreampEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *voicing, *gain, *bass, *middle, *treble, *presence, *bright, *cab, *oversampling;
    PreampEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKPreampProcessor)
};
