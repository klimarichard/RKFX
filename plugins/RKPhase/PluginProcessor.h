#pragma once

#include "plugin/ProcessorBase.h"

#include "Parameters.h"
#include "PhaseEngine.h"

/** RKPhase: phaser with 2 to 12 stages, feedback, LFO and envelope. */
class RKPhaseProcessor : public rkfx::ProcessorBase
{
public:
    RKPhaseProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    PhaseEngine::Settings currentSettings() const;

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

private:
    juce::RangedAudioParameter *stages, *centre, *feedback, *rate, *depth, *sync, *division, *phase,
                               *envAmount, *envSens, *envAttack, *envRelease;
    PhaseEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKPhaseProcessor)
};
