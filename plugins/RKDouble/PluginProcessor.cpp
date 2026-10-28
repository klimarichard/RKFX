#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKDouble";
        c.stateVersion = 1;
        c.hasMix = false;                     // the dry signal always stays at full level
        c.monoToStereo = true;                // spread places dry and double apart
        c.accent = juce::Colour (0xff6fa3d8); // Modulation family, steel blue
        c.family = rkfx::Palette::Family::modulation;
        return c;
    }
}

RKDoubleProcessor::RKDoubleProcessor()
    : rkfx::ProcessorBase (makeConfig(), DoubleParams::create()),
      delay (findParam ("delay")), drift (findParam ("drift")), driftSpeed (findParam ("drift_speed")),
      tone (findParam ("tone")), spread (findParam ("spread")), level (findParam ("level"))
{
}

DoubleEngine::Settings RKDoubleProcessor::currentSettings() const
{
    DoubleEngine::Settings s;
    s.delayMs = valueOf (delay);
    s.drift   = valueOf (drift) / 100.0f;
    s.driftHz = valueOf (driftSpeed);
    s.tone    = valueOf (tone) / 100.0f;
    s.spread  = valueOf (spread) / 100.0f;
    s.levelDb = valueOf (level);
    return s;
}

void RKDoubleProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKDoubleProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKDoubleProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKDoubleProcessor::createEditor()
{
    return new RKDoubleEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKDoubleProcessor();
}
#endif
