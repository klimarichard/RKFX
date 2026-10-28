#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "VocodeEngine.h"

/** RKVocode: channel vocoder with a sidechain or an internal, MIDI-playable
    carrier. */
class RKVocodeProcessor : public rkfx::ProcessorBase
{
public:
    RKVocodeProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    VocodeEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *bands, *freqLow, *freqHigh, *formant, *attack, *release, *unvoiced, *modMix,
                               *carrier, *oscWave, *oscMode, *oscNote, *oscVoices, *oscGlide;
    VocodeEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVocodeProcessor)
};
