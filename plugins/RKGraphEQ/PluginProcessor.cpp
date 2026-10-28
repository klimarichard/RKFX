#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKGraphEQ";
        c.stateVersion = 1;
        c.hasMix = false;                     // mixing an EQ with the dry signal colours the phase
        c.accent = juce::Colour (0xffa77ce8); // Filter and EQ family, lavender
        c.family = rkfx::Palette::Family::filterEQ;
        return c;
    }
}

RKGraphEQProcessor::RKGraphEQProcessor()
    : rkfx::ProcessorBase (makeConfig(), GraphEQParams::create()),
      bands (findParam ("bands"))
{
    for (int m = 0; m < 3; ++m)
        for (int b = 0; b < GraphEQParams::modeCount (m); ++b)
            gains[(size_t) m][(size_t) b] = findParam (GraphEQParams::bandId (m, b));
}

int RKGraphEQProcessor::activeMode() const
{
    return juce::jlimit (0, 2, juce::roundToInt (valueOf (bands)));
}

GraphEQEngine::Settings RKGraphEQProcessor::currentSettings() const
{
    GraphEQEngine::Settings s;
    s.mode = GraphEQParams::modes[activeMode()];

    for (int m = 0; m < 3; ++m)
        for (int b = 0; b < GraphEQParams::modeCount (m); ++b)
            s.gainsDb[(size_t) m][(size_t) b] = valueOf (gains[(size_t) m][(size_t) b]);

    return s;
}

void RKGraphEQProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKGraphEQProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKGraphEQProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKGraphEQProcessor::createEditor()
{
    return new RKGraphEQEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKGraphEQProcessor();
}
