#pragma once

#include "plugin/ProcessorBase.h"

#include "ArpEngine.h"
#include "Parameters.h"

/** RKArpeggiate: a tempo-synced arpeggiator with latch (MIDI effect). */
class RKArpeggiateProcessor : public rkfx::ProcessorBase
{
public:
    RKArpeggiateProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    ArpEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int, int) override { engine.prepare (sampleRate); }
    void resetDsp() override                               { engine.reset(); }
    void processDsp (juce::AudioBuffer<float>&) override {}
    void processMidi (juce::MidiBuffer&, int numSamples) override;
    void releaseMidi (juce::MidiBuffer& midi) override { engine.releaseAll (midi); }

private:
    juce::RangedAudioParameter *pattern, *division, *octaves, *gate, *swing, *latch, *velocityMode, *velocity;
    ArpEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKArpeggiateProcessor)
};
