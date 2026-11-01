#pragma once

#include "plugin/ProcessorBase.h"

#include "HarmonizeEngine.h"
#include "Parameters.h"

/** RKHarmonize: up to two harmony notes in a key and scale (MIDI effect). */
class RKHarmonizeProcessor : public rkfx::ProcessorBase
{
public:
    RKHarmonizeProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    HarmonizeEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double, int, int) override { engine.prepare(); }
    void resetDsp() override                    { engine.reset(); }
    void processDsp (juce::AudioBuffer<float>&) override {}
    void processMidi (juce::MidiBuffer&, int numSamples) override;
    void releaseMidi (juce::MidiBuffer& midi) override { engine.releaseAll (midi); }
    void migrateParams (juce::DynamicObject& params, int fromVersion) override;

private:
    juce::RangedAudioParameter *key, *scale, *voice1, *voice2, *velocity, *original, *semitones1, *semitones2;
    HarmonizeEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKHarmonizeProcessor)
};
