#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKPolyPitch";
        c.stateVersion = 1;
        c.hasMix = false;                     // the modes are the mix
        c.monoToStereo = true;                // Detune spreads its voices
        c.accent = juce::Colour (0xff5468d4); // Modulation family, indigo blue
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKPolyPitchProcessor::RKPolyPitchProcessor()
    : rkfx::ProcessorBase (makeConfig(), PolyPitchParams::create()),
      mode (findParam ("mode")), range (findParam ("range")), heel (findParam ("heel")), toe (findParam ("toe")),
      pedal (findParam ("pedal")), glide (findParam ("glide")), momentary (findParam ("momentary")), rise (findParam ("rise")),
      fall (findParam ("fall")), fine (findParam ("fine")), detune (findParam ("detune")), voiceLevel (findParam ("voice_level")),
      width (findParam ("width")), tone (findParam ("tone")), lowLatency (findParam ("low_latency"))
{
}

PolyPitchEngine::Settings RKPolyPitchProcessor::currentSettings() const
{
    PolyPitchEngine::Settings s;
    s.mode        = (PolyPitchEngine::Mode) juce::jlimit (0, 2, juce::roundToInt (valueOf (mode)));
    s.range       = (PolyPitchEngine::Range) juce::jlimit (0, 2, juce::roundToInt (valueOf (range)));
    s.heel        = valueOf (heel);
    s.toe         = valueOf (toe);
    s.pedal       = valueOf (pedal) / 100.0f;
    s.glideMs     = valueOf (glide);
    s.momentary   = valueOf (momentary) >= 0.5f;
    s.riseMs      = valueOf (rise);
    s.fallMs      = valueOf (fall);
    s.fineCents   = valueOf (fine);
    s.detuneCents = valueOf (detune);
    s.voiceGainDb = valueOf (voiceLevel);
    s.width       = valueOf (width) / 100.0f;
    s.toneHz      = valueOf (tone);
    s.lowLatency  = valueOf (lowLatency) >= 0.5f;
    return s;
}

void RKPolyPitchProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKPolyPitchProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKPolyPitchProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    shownShift = engine.getShift();
    shownPosition = engine.getPosition();
}

juce::AudioProcessorEditor* RKPolyPitchProcessor::createEditor()
{
    return new RKPolyPitchEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKPolyPitchProcessor();
}
#endif
