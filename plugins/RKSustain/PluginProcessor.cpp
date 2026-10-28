#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKSustain";
        c.stateVersion = 1;
        c.midiEffect = true;
        c.accent = juce::Colour (0xffef8fc7); // MIDI family, light pink
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKSustainProcessor::RKSustainProcessor()
    : rkfx::ProcessorBase (makeConfig(), SustainParams::create()),
      mode (findParam ("mode")), pedal (findParam ("pedal")), useCC (findParam ("use_cc")), cc (findParam ("cc"))
{
}

SustainEngine::Settings RKSustainProcessor::currentSettings() const
{
    SustainEngine::Settings s;
    s.mode           = (SustainEngine::Mode) juce::jlimit (0, 3, juce::roundToInt (valueOf (mode)));
    s.pedalParameter = valueOf (pedal) >= 0.5f;
    s.controller     = valueOf (useCC) >= 0.5f ? juce::jlimit (0, 127, juce::roundToInt (valueOf (cc))) : -1;
    return s;
}

void RKSustainProcessor::processMidi (juce::MidiBuffer& midi, int)
{
    engine.setSettings (currentSettings());
    engine.process (midi);
}

juce::AudioProcessorEditor* RKSustainProcessor::createEditor()
{
    return new RKSustainEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKSustainProcessor();
}
#endif
