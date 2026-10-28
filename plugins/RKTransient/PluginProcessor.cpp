#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKTransient";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 100.0f;
        c.accent = juce::Colour (0xffe8b923); // Dynamics family, mustard
        return c;
    }
}

RKTransientProcessor::RKTransientProcessor()
    : rkfx::ProcessorBase (makeConfig(), TransientParams::create()),
      attack (findParam ("attack")), sustain (findParam ("sustain")), attackTime (findParam ("attack_time")),
      sustainTime (findParam ("sustain_time")), focusHpf (findParam ("focus_hpf")), focusLpf (findParam ("focus_lpf")),
      focusListen (findParam ("focus_listen")), link (findParam ("link"))
{
}

TransientEngine::Settings RKTransientProcessor::currentSettings() const
{
    TransientEngine::Settings s;
    s.attack          = valueOf (attack) / 100.0f;
    s.sustain         = valueOf (sustain) / 100.0f;
    s.attackTimeMs    = valueOf (attackTime);
    s.sustainTimeMs   = valueOf (sustainTime);
    s.focusHighPassHz = valueOf (focusHpf);
    s.focusLowPassHz  = valueOf (focusLpf);
    s.listen          = valueOf (focusListen) >= 0.5f;
    s.link            = valueOf (link) >= 0.5f;
    return s;
}

void RKTransientProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKTransientProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKTransientProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKTransientProcessor::createEditor()
{
    return new RKTransientEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKTransientProcessor();
}
