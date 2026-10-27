#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKPhase";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 50.0f;                 // equal blend = deepest notches
        c.monoToStereo = true;                // stereo phase on a mono source
        c.accent = juce::Colour (0xff8ad6f2); // Modulation family, pale blue
        return c;
    }
}

RKPhaseProcessor::RKPhaseProcessor()
    : rkfx::ProcessorBase (makeConfig(), PhaseParams::create()),
      stages (findParam ("stages")), centre (findParam ("center")), feedback (findParam ("feedback")),
      rate (findParam ("rate")), depth (findParam ("depth")), sync (findParam ("sync")),
      division (findParam ("division")), phase (findParam ("phase")), envAmount (findParam ("env_amount")),
      envSens (findParam ("env_sens")), envAttack (findParam ("env_attack")), envRelease (findParam ("env_release"))
{
}

PhaseEngine::Settings RKPhaseProcessor::currentSettings() const
{
    PhaseEngine::Settings s;
    s.stages       = PhaseParams::stageCounts[juce::jlimit (0, 4, juce::roundToInt (valueOf (stages)))];
    s.centreHz     = valueOf (centre);
    s.feedback     = valueOf (feedback) / 100.0f;
    s.rateHz       = valueOf (rate);
    s.depth        = valueOf (depth) / 100.0f;
    s.sync         = valueOf (sync) >= 0.5f;
    s.division     = juce::roundToInt (valueOf (division));
    s.phaseDeg     = valueOf (phase);
    s.envAmount    = valueOf (envAmount) / 100.0f;
    s.envSensDb    = valueOf (envSens);
    s.envAttackMs  = valueOf (envAttack);
    s.envReleaseMs = valueOf (envRelease);
    return s;
}

void RKPhaseProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKPhaseProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKPhaseProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), getTransport());
}

juce::AudioProcessorEditor* RKPhaseProcessor::createEditor()
{
    return new RKPhaseEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKPhaseProcessor();
}
