#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "SustainEngine.h"

/** RKSustain: sustain, sostenuto, hold-until-next-chord and latch (MIDI effect). */
class RKSustainProcessor : public rkfx::ProcessorBase
{
public:
    RKSustainProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    SustainEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double, int, int) override { engine.prepare(); }
    void resetDsp() override                    { engine.reset(); }
    void processDsp (juce::AudioBuffer<float>&) override {}
    void processMidi (juce::MidiBuffer&, int numSamples) override;
    void releaseMidi (juce::MidiBuffer& midi) override { engine.releaseAll (midi); }

private:
    juce::RangedAudioParameter *mode, *pedal, *useCC, *cc;
    SustainEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKSustainProcessor)
};
