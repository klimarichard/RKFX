#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKStrum";
        c.stateVersion = 1;
        c.midiEffect = true;
        c.accent = juce::Colour (0xffc23f86); // MIDI family, berry
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKStrumProcessor::RKStrumProcessor()
    : rkfx::ProcessorBase (makeConfig(), StrumParams::create()),
      direction (findParam ("direction")), speed (findParam ("speed")), window (findParam ("window")),
      ramp (findParam ("ramp")), humanize (findParam ("humanize"))
{
}

StrumEngine::Settings RKStrumProcessor::currentSettings() const
{
    StrumEngine::Settings s;
    s.direction = (StrumEngine::Direction) juce::jlimit (0, 2, juce::roundToInt (valueOf (direction)));
    s.speedMs   = valueOf (speed);
    s.windowMs  = valueOf (window);
    s.ramp      = valueOf (ramp) / 100.0f;
    s.humanize  = valueOf (humanize) / 100.0f;
    return s;
}

void RKStrumProcessor::processMidi (juce::MidiBuffer& midi, int numSamples)
{
    engine.setSettings (currentSettings());
    engine.process (midi, numSamples);
}

juce::AudioProcessorEditor* RKStrumProcessor::createEditor()
{
    return new RKStrumEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKStrumProcessor();
}
#endif
