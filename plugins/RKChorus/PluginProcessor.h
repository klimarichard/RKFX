#pragma once

#include "plugin/ProcessorBase.h"

#include "ChorusEngine.h"
#include "Parameters.h"

/** RKChorus: 1 to 4 voices per side, from pedal chorus to lush ensemble. */
class RKChorusProcessor : public rkfx::ProcessorBase
{
public:
    RKChorusProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    ChorusEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *voices, *rate, *depth, *delay, *width, *tone, *sync, *division;
    ChorusEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKChorusProcessor)
};
