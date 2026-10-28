#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKSwell";
        c.stateVersion = 1;
        c.hasMix = false;
        c.accent = juce::Colour (0xffc9961a); // Dynamics family, ochre
        c.family = rkfx::Palette::Family::dynamics;
        return c;
    }
}

RKSwellProcessor::RKSwellProcessor()
    : rkfx::ProcessorBase (makeConfig(), SwellParams::create()),
      threshold (findParam ("threshold")), time (findParam ("time")), curve (findParam ("curve")),
      retrigger (findParam ("retrigger")), link (findParam ("link"))
{
}

SwellEngine::Settings RKSwellProcessor::currentSettings() const
{
    SwellEngine::Settings s;
    s.thresholdDb = valueOf (threshold);
    s.timeMs      = valueOf (time);
    s.curve       = (SwellEngine::Curve) juce::jlimit (0, 2, juce::roundToInt (valueOf (curve)));
    s.retrigger   = valueOf (retrigger) / 100.0f;
    s.link        = valueOf (link) >= 0.5f;
    return s;
}

void RKSwellProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKSwellProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKSwellProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKSwellProcessor::createEditor()
{
    return new RKSwellEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKSwellProcessor();
}
#endif
