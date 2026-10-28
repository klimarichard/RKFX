#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKChorus";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 50.0f;
        c.monoToStereo = true;                // mono in, wide stereo chorus out
        c.accent = juce::Colour (0xff5cc2ec); // Modulation family, light sky
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKChorusProcessor::RKChorusProcessor()
    : rkfx::ProcessorBase (makeConfig(), ChorusParams::create()),
      voices (findParam ("voices")), rate (findParam ("rate")), depth (findParam ("depth")),
      delay (findParam ("delay")), width (findParam ("width")), tone (findParam ("tone")),
      sync (findParam ("sync")), division (findParam ("division"))
{
}

ChorusEngine::Settings RKChorusProcessor::currentSettings() const
{
    ChorusEngine::Settings s;
    s.voices   = juce::roundToInt (valueOf (voices));
    s.rateHz   = valueOf (rate);
    s.depth    = valueOf (depth) / 100.0f;
    s.delayMs  = valueOf (delay);
    s.width    = valueOf (width) / 100.0f;
    s.tone     = valueOf (tone) / 100.0f;
    s.sync     = valueOf (sync) >= 0.5f;
    s.division = juce::roundToInt (valueOf (division));
    return s;
}

void RKChorusProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKChorusProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKChorusProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), getTransport());
}

juce::AudioProcessorEditor* RKChorusProcessor::createEditor()
{
    return new RKChorusEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKChorusProcessor();
}
