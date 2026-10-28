#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKOctave";
        c.stateVersion = 1;
        c.hasMix = false;                     // Direct and the octave levels are the mix
        c.accent = juce::Colour (0xff4a6fe3); // Modulation family, royal blue
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKOctaveProcessor::RKOctaveProcessor()
    : rkfx::ProcessorBase (makeConfig(), OctaveParams::create()),
      range (findParam ("range")), voice (findParam ("voice")), direct (findParam ("direct")),
      octave1 (findParam ("octave1")), octave2 (findParam ("octave2")), tone (findParam ("tone")),
      threshold (findParam ("threshold"))
{
}

OctaveEngine::Settings RKOctaveProcessor::currentSettings() const
{
    OctaveEngine::Settings s;
    s.range       = (OctaveEngine::Range) juce::jlimit (0, 2, juce::roundToInt (valueOf (range)));
    s.voice       = (OctaveEngine::Voice) juce::jlimit (0, 1, juce::roundToInt (valueOf (voice)));
    s.direct      = valueOf (direct) / 100.0f;
    s.octave1     = valueOf (octave1) / 100.0f;
    s.octave2     = valueOf (octave2) / 100.0f;
    s.toneHz      = valueOf (tone);
    s.thresholdDb = valueOf (threshold);
    return s;
}

void RKOctaveProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKOctaveProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKOctaveProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKOctaveProcessor::createEditor()
{
    return new RKOctaveEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKOctaveProcessor();
}
#endif
