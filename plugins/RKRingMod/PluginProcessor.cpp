#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKRingMod";
        c.stateVersion = 1;
        c.hasMix = true;
        c.mixDefault = 100.0f;
        c.accent = juce::Colour (0xff4f8fe0); // Modulation family, azure
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKRingModProcessor::RKRingModProcessor()
    : rkfx::ProcessorBase (makeConfig(), RingModParams::create()),
      mode (findParam ("mode")), wave (findParam ("wave")), frequency (findParam ("frequency")),
      interval (findParam ("interval")), division (findParam ("division")), range (findParam ("range")),
      lfoDepth (findParam ("lfo_depth")), lfoRate (findParam ("lfo_rate"))
{
}

RingModEngine::Settings RKRingModProcessor::currentSettings() const
{
    RingModEngine::Settings s;
    s.mode              = (RingModEngine::Mode) juce::jlimit (0, 2, juce::roundToInt (valueOf (mode)));
    s.wave              = (RingModEngine::Wave) juce::jlimit (0, 2, juce::roundToInt (valueOf (wave)));
    s.frequencyHz       = valueOf (frequency);
    s.intervalSemitones = valueOf (interval);
    s.division          = juce::jlimit (0, rkfx::SyncDivision::kCount - 1, juce::roundToInt (valueOf (division)));
    s.range             = (RingModEngine::Range) juce::jlimit (0, 2, juce::roundToInt (valueOf (range)));
    s.lfoDepth          = valueOf (lfoDepth) / 100.0f;
    s.lfoRateHz         = valueOf (lfoRate);
    return s;
}

void RKRingModProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKRingModProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKRingModProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), getTransport());
}

juce::AudioProcessorEditor* RKRingModProcessor::createEditor()
{
    return new RKRingModEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKRingModProcessor();
}
#endif
