#pragma once

#include "plugin/ProcessorBase.h"

#include "EchoEngine.h"
#include "Parameters.h"

#include <atomic>

/** RKEcho: a MIDI echo. Repeats the notes played in time, quieter,
    transposed and on other channels if set (MIDI effect). */
class RKEchoProcessor : public rkfx::ProcessorBase
{
public:
    RKEchoProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    EchoEngine::Settings currentSettings() const;

    /** The last note played (for the display), -1 before any. */
    std::atomic<int> lastNote { -1 }, lastVelocity { 100 };

protected:
    void prepareDsp (double sampleRate, int, int) override { engine.prepare (sampleRate); }
    void resetDsp() override                               { engine.reset(); }
    void processDsp (juce::AudioBuffer<float>&) override {}
    void processMidi (juce::MidiBuffer&, int numSamples) override;
    void releaseMidi (juce::MidiBuffer& midi) override { engine.releaseAll (midi); }

private:
    juce::RangedAudioParameter *time, *sync, *division, *repeats, *feedback, *velMin, *step, *scaleSteps, *key, *scale,
                               *length, *gate, *swing, *original, *channel, *alternate;
    EchoEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKEchoProcessor)
};
