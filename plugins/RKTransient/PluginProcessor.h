#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "TransientEngine.h"

/** RKTransient: level-independent transient shaper. */
class RKTransientProcessor : public rkfx::ProcessorBase
{
public:
    RKTransientProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    TransientEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *attack, *sustain, *attackTime, *sustainTime, *focusHpf, *focusLpf, *focusListen, *link;
    TransientEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKTransientProcessor)
};
