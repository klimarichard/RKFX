#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKPreamp";
        c.stateVersion = 1;
        c.hasMix = true;                      // also for blending a clean signal in
        c.mixDefault = 100.0f;
        c.accent = juce::Colour (0xffb33c28); // Drive family, rust red
        c.family = rkfx::Palette::Family::drive;
        return c;
    }
}

RKPreampProcessor::RKPreampProcessor()
    : rkfx::ProcessorBase (makeConfig(), PreampParams::create()),
      voicing (findParam ("voicing")), gain (findParam ("gain")), bass (findParam ("bass")),
      middle (findParam ("middle")), treble (findParam ("treble")), presence (findParam ("presence")),
      bright (findParam ("bright")), cab (findParam ("cab")), oversampling (findParam ("oversampling"))
{
    reportLatency (rkfx::Oversampler::latencySamples (currentSettings().oversampling));
}

PreampEngine::Settings RKPreampProcessor::currentSettings() const
{
    PreampEngine::Settings s;
    s.voicing      = (PreampEngine::Voicing) juce::jlimit (0, 3, juce::roundToInt (valueOf (voicing)));
    s.gain         = valueOf (gain) / 10.0f;
    s.bass         = valueOf (bass) / 10.0f;
    s.middle       = valueOf (middle) / 10.0f;
    s.treble       = valueOf (treble) / 10.0f;
    s.presence     = valueOf (presence) / 10.0f;
    s.bright       = valueOf (bright) >= 0.5f;
    s.cab          = (rkfx::CabType) juce::jlimit (0, 4, juce::roundToInt (valueOf (cab)));
    s.oversampling = PreampParams::oversamplingFactors[juce::jlimit (0, 2, juce::roundToInt (valueOf (oversampling)))];
    return s;
}

void RKPreampProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
    reportLatency (engine.latencySamples());
}

void RKPreampProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKPreampProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    reportLatency (engine.latencySamples());
}

juce::AudioProcessorEditor* RKPreampProcessor::createEditor()
{
    return new RKPreampEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKPreampProcessor();
}
#endif
