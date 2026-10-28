#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKFilter";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 100.0f;
        c.accent = juce::Colour (0xff8e5bd9); // Filter and EQ family, violet
        c.family = rkfx::Palette::Family::filterEQ;
        return c;
    }
}

RKFilterProcessor::RKFilterProcessor()
    : rkfx::ProcessorBase (makeConfig(), FilterParams::create()),
      mode (findParam ("mode")), slope (findParam ("slope")),
      cutoff (findParam ("cutoff")), resonance (findParam ("resonance")),
      envAmount (findParam ("env_amount")), envSens (findParam ("env_sens")),
      envAttack (findParam ("env_attack")), envRelease (findParam ("env_release")),
      lfoDepth (findParam ("lfo_depth")), lfoShape (findParam ("lfo_shape")),
      lfoRate (findParam ("lfo_rate")), lfoSync (findParam ("lfo_sync")),
      lfoDivision (findParam ("lfo_division")), lfoPhase (findParam ("lfo_phase"))
{
}

void RKFilterProcessor::updateEngine()
{
    FilterEngine::Settings s;
    s.mode         = (FilterEngine::Mode) juce::roundToInt (valueOf (mode));
    s.stages       = juce::roundToInt (valueOf (slope)) + 1;
    s.cutoffHz     = valueOf (cutoff);
    s.resonance    = valueOf (resonance) / 100.0f;
    s.envAmount    = valueOf (envAmount) / 100.0f;
    s.envSensDb    = valueOf (envSens);
    s.envAttackMs  = valueOf (envAttack);
    s.envReleaseMs = valueOf (envRelease);
    s.lfoDepth     = valueOf (lfoDepth) / 100.0f;
    s.lfoShape     = (rkfx::LFO::Shape) juce::roundToInt (valueOf (lfoShape));
    s.lfoRateHz    = valueOf (lfoRate);
    s.lfoSync      = valueOf (lfoSync) >= 0.5f;
    s.lfoDivision  = juce::roundToInt (valueOf (lfoDivision));
    s.lfoPhaseDeg  = valueOf (lfoPhase);
    engine.setSettings (s);
}

void RKFilterProcessor::prepareDsp (double sampleRate, int, int)
{
    updateEngine();
    engine.prepare (sampleRate);
}

void RKFilterProcessor::resetDsp()
{
    updateEngine();
    engine.reset();
}

void RKFilterProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    updateEngine();
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), getTransport());
}

juce::AudioProcessorEditor* RKFilterProcessor::createEditor()
{
    return new RKFilterEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKFilterProcessor();
}
#endif
