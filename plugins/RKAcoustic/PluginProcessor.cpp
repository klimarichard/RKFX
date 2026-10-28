#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKAcoustic";
        c.stateVersion = 1;
        c.hasMix = false;                     // a preamp shapes the whole signal
        c.accent = juce::Colour (0xff7c52d1); // Filter and EQ family, amethyst
        c.family = rkfx::Palette::Family::filterEQ;
        return c;
    }
}

RKAcousticProcessor::RKAcousticProcessor()
    : rkfx::ProcessorBase (makeConfig(), AcousticParams::create()),
      voicing (findParam ("voicing")), body (findParam ("body")), quack (findParam ("quack")),
      highPass (findParam ("highpass")), phase (findParam ("phase")), low (findParam ("low")),
      mid (findParam ("mid")), midFreq (findParam ("mid_freq")), treble (findParam ("treble")),
      air (findParam ("air")), notch (findParam ("notch")), compress (findParam ("compress"))
{
}

AcousticEngine::Settings RKAcousticProcessor::currentSettings() const
{
    AcousticEngine::Settings s;
    s.voicing    = (AcousticEngine::Voicing) juce::jlimit (0, 4, juce::roundToInt (valueOf (voicing)));
    s.body       = valueOf (body) / 100.0f;
    s.quack      = valueOf (quack) / 100.0f;
    s.highPassHz = valueOf (highPass);
    s.invert     = valueOf (phase) >= 0.5f;
    s.lowDb      = valueOf (low);
    s.midDb      = valueOf (mid);
    s.midHz      = valueOf (midFreq);
    s.trebleDb   = valueOf (treble);
    s.airDb      = valueOf (air);
    s.notchHz    = valueOf (notch);
    s.compress   = valueOf (compress) / 100.0f;
    return s;
}

void RKAcousticProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKAcousticProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKAcousticProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKAcousticProcessor::createEditor()
{
    return new RKAcousticEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKAcousticProcessor();
}
#endif
