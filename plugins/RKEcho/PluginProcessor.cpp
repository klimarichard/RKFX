#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKEcho";
        c.stateVersion = 1;
        c.midiEffect = true;
        c.accent = juce::Colour (0xffd2457c); // MIDI family, rose
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKEchoProcessor::RKEchoProcessor()
    : rkfx::ProcessorBase (makeConfig(), EchoParams::create()),
      time (findParam ("time")), sync (findParam ("sync")), division (findParam ("division")), repeats (findParam ("repeats")),
      feedback (findParam ("feedback")), velMin (findParam ("vel_min")), step (findParam ("step")),
      scaleSteps (findParam ("scale_steps")), key (findParam ("key")), scale (findParam ("scale")), length (findParam ("length")),
      gate (findParam ("gate")), swing (findParam ("swing")), original (findParam ("original")), channel (findParam ("channel")),
      alternate (findParam ("alternate"))
{
}

EchoEngine::Settings RKEchoProcessor::currentSettings() const
{
    EchoEngine::Settings s;
    s.timeMs      = valueOf (time);
    s.sync        = valueOf (sync) >= 0.5f;
    s.division    = juce::jlimit (0, rkfx::SyncDivision::kCount - 1, juce::roundToInt (valueOf (division)));
    s.repeats     = juce::roundToInt (valueOf (repeats));
    s.feedback    = valueOf (feedback) / 100.0f;
    s.velocityMin = juce::roundToInt (valueOf (velMin));
    s.step        = juce::roundToInt (valueOf (step));
    s.scaleSteps  = valueOf (scaleSteps) >= 0.5f;
    s.key         = juce::jlimit (0, 11, juce::roundToInt (valueOf (key)));
    s.scale       = (rkfx::scales::Scale) juce::jlimit (0, rkfx::scales::kNumScales - 1, juce::roundToInt (valueOf (scale)));
    s.asPlayed    = valueOf (length) < 0.5f;
    s.gate        = valueOf (gate) / 100.0f;
    s.swing       = valueOf (swing) / 100.0f;
    s.original    = valueOf (original) >= 0.5f;
    s.channel     = juce::roundToInt (valueOf (channel));
    s.alternate   = juce::roundToInt (valueOf (alternate));
    return s;
}

void RKEchoProcessor::processMidi (juce::MidiBuffer& midi, int numSamples)
{
    engine.setSettings (currentSettings());
    engine.process (midi, numSamples, getTransport());
    lastNote = engine.getLastNote();
    lastVelocity = engine.getLastVelocity();
}

juce::AudioProcessorEditor* RKEchoProcessor::createEditor()
{
    return new RKEchoEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKEchoProcessor();
}
#endif
