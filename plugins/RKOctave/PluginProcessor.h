#pragma once

#include "plugin/ProcessorBase.h"

#include "OctaveEngine.h"
#include "Parameters.h"

/** RKOctave: analog-style octave divider, one and two octaves down. */
class RKOctaveProcessor : public rkfx::ProcessorBase
{
public:
    RKOctaveProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    OctaveEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *range, *voice, *direct, *octave1, *octave2, *tone, *threshold;
    OctaveEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKOctaveProcessor)
};
