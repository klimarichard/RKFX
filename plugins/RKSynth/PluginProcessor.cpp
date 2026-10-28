#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKSynth";
        c.stateVersion = 1;
        c.hasMix = true;                      // blend the instrument under the synth
        c.mixDefault = 100.0f;
        c.accent = juce::Colour (0xff2f6fb5); // Modulation family, cobalt
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKSynthProcessor::RKSynthProcessor()
    : rkfx::ProcessorBase (makeConfig(), SynthParams::create()),
      range (findParam ("range")), wave (findParam ("wave")), octave (findParam ("octave")), sub (findParam ("sub")),
      glide (findParam ("glide")), cutoff (findParam ("cutoff")), resonance (findParam ("resonance")),
      envAmount (findParam ("env_amount")), decay (findParam ("decay")), dynamics (findParam ("dynamics")),
      threshold (findParam ("threshold"))
{
}

SynthEngine::Settings RKSynthProcessor::currentSettings() const
{
    SynthEngine::Settings s;
    s.range       = (SynthEngine::Range) juce::jlimit (0, 2, juce::roundToInt (valueOf (range)));
    s.wave        = (SynthEngine::Wave) juce::jlimit (0, 2, juce::roundToInt (valueOf (wave)));
    s.octave      = SynthParams::octaveShifts[juce::jlimit (0, 3, juce::roundToInt (valueOf (octave)))];
    s.sub         = valueOf (sub) / 100.0f;
    s.glideMs     = valueOf (glide);
    s.cutoffHz    = valueOf (cutoff);
    s.resonance   = valueOf (resonance) / 100.0f;
    s.envAmount   = valueOf (envAmount) / 100.0f;
    s.decayMs     = valueOf (decay);
    s.dynamics    = valueOf (dynamics) / 100.0f;
    s.thresholdDb = valueOf (threshold);
    return s;
}

void RKSynthProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKSynthProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKSynthProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKSynthProcessor::createEditor()
{
    return new RKSynthEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKSynthProcessor();
}
#endif
