#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "StrumEngine.h"

/** RKStrum: chords spread out in time like a strummed guitar (MIDI effect). */
class RKStrumProcessor : public rkfx::ProcessorBase
{
public:
    RKStrumProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    StrumEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int, int) override { engine.prepare (sampleRate); }
    void resetDsp() override                               { engine.reset(); }
    void processDsp (juce::AudioBuffer<float>&) override {}
    void processMidi (juce::MidiBuffer&, int numSamples) override;
    void releaseMidi (juce::MidiBuffer& midi) override { engine.releaseAll (midi); }

private:
    juce::RangedAudioParameter *direction, *speed, *window, *ramp, *humanize;
    StrumEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKStrumProcessor)
};
