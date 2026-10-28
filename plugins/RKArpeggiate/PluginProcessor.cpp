#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKArpeggiate";
        c.stateVersion = 1;
        c.midiEffect = true;
        c.accent = juce::Colour (0xffe36fb4); // MIDI family, pink
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKArpeggiateProcessor::RKArpeggiateProcessor()
    : rkfx::ProcessorBase (makeConfig(), ArpParams::create()),
      pattern (findParam ("pattern")), division (findParam ("division")), octaves (findParam ("octaves")),
      gate (findParam ("gate")), swing (findParam ("swing")), latch (findParam ("latch")),
      velocityMode (findParam ("velocity_mode")), velocity (findParam ("velocity"))
{
}

ArpEngine::Settings RKArpeggiateProcessor::currentSettings() const
{
    ArpEngine::Settings s;
    s.pattern       = (ArpEngine::Pattern) juce::jlimit (0, 5, juce::roundToInt (valueOf (pattern)));
    s.division      = juce::jlimit (0, rkfx::SyncDivision::kCount - 1, juce::roundToInt (valueOf (division)));
    s.octaves       = juce::jlimit (1, 4, juce::roundToInt (valueOf (octaves)));
    s.gate          = valueOf (gate) / 100.0f;
    s.swing         = valueOf (swing) / 100.0f;
    s.latch         = valueOf (latch) >= 0.5f;
    s.fixedVelocity = valueOf (velocityMode) >= 0.5f;
    s.velocity      = juce::jlimit (1, 127, juce::roundToInt (valueOf (velocity)));
    return s;
}

void RKArpeggiateProcessor::processMidi (juce::MidiBuffer& midi, int numSamples)
{
    engine.setSettings (currentSettings());
    engine.process (midi, numSamples, getTransport());
}

juce::AudioProcessorEditor* RKArpeggiateProcessor::createEditor()
{
    return new RKArpeggiateEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKArpeggiateProcessor();
}
#endif
