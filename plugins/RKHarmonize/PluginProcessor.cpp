#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKHarmonize";
        c.stateVersion = 1;
        c.midiEffect = true;
        c.accent = juce::Colour (0xffd9509a); // MIDI family, magenta
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKHarmonizeProcessor::RKHarmonizeProcessor()
    : rkfx::ProcessorBase (makeConfig(), HarmonizeParams::create()),
      key (findParam ("key")), scale (findParam ("scale")), voice1 (findParam ("voice1")), voice2 (findParam ("voice2")),
      velocity (findParam ("velocity")), original (findParam ("original"))
{
}

HarmonizeEngine::Settings RKHarmonizeProcessor::currentSettings() const
{
    using Interval = HarmonizeEngine::Interval;
    HarmonizeEngine::Settings s;
    s.key      = juce::jlimit (0, 11, juce::roundToInt (valueOf (key)));
    s.scale    = (HarmonizeEngine::Scale) juce::jlimit (0, HarmonizeEngine::kNumScales - 1, juce::roundToInt (valueOf (scale)));
    s.voice1   = (Interval) juce::jlimit (0, HarmonizeEngine::kNumIntervals - 1, juce::roundToInt (valueOf (voice1)));
    s.voice2   = (Interval) juce::jlimit (0, HarmonizeEngine::kNumIntervals - 1, juce::roundToInt (valueOf (voice2)));
    s.velocity = valueOf (velocity) / 100.0f;
    s.original = valueOf (original) >= 0.5f;
    return s;
}

void RKHarmonizeProcessor::processMidi (juce::MidiBuffer& midi, int)
{
    engine.setSettings (currentSettings());
    engine.process (midi);
}

juce::AudioProcessorEditor* RKHarmonizeProcessor::createEditor()
{
    return new RKHarmonizeEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKHarmonizeProcessor();
}
#endif
