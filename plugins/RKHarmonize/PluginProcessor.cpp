#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKHarmonize";
        c.stateVersion = 2;   // 2: free chromatic semitones (voice1_semis, voice2_semis)
        c.midiEffect = true;
        c.accent = juce::Colour (0xffd9509a); // MIDI family, magenta
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKHarmonizeProcessor::RKHarmonizeProcessor()
    : rkfx::ProcessorBase (makeConfig(), HarmonizeParams::create()),
      key (findParam ("key")), scale (findParam ("scale")), voice1 (findParam ("voice1")), voice2 (findParam ("voice2")),
      velocity (findParam ("velocity")), original (findParam ("original")),
      semitones1 (findParam ("voice1_semis")), semitones2 (findParam ("voice2_semis"))
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
    s.semitones1 = juce::jlimit (-12, 12, juce::roundToInt (valueOf (semitones1)));
    s.semitones2 = juce::jlimit (-12, 12, juce::roundToInt (valueOf (semitones2)));
    return s;
}

void RKHarmonizeProcessor::migrateParams (juce::DynamicObject& params, int fromVersion)
{
    // Before version 2 the chromatic scale used the Voice intervals as fixed
    // semitone steps (a 3rd = 4). Carry them over so old settings sound the same.
    if (fromVersion < 2 && params.getProperty ("scale").toString() == "chromatic")
    {
        const auto carry = [&params] (const char* voiceId, const char* semitonesId, const char* defaultKey)
        {
            const auto stored = params.getProperty (voiceId).toString();
            const auto index = HarmonizeParams::intervalKeys.indexOf (stored.isEmpty() ? juce::String (defaultKey) : stored);
            if (index >= 0)
                params.setProperty (semitonesId, rkfx::scales::semitonesOf ((HarmonizeEngine::Interval) index));
        };
        carry ("voice1", "voice1_semis", "3rd_above");
        carry ("voice2", "voice2_semis", "off");
    }
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
