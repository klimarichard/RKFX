#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKParaEQ";
        c.stateVersion = 1;
        c.hasMix = false;                     // mixing an EQ with the dry signal colours the phase
        c.accent = juce::Colour (0xff6e45c4); // Filter and EQ family, deep purple
        c.family = rkfx::Palette::Family::filterEQ;
        return c;
    }
}

RKParaEQProcessor::RKParaEQProcessor()
    : rkfx::ProcessorBase (makeConfig(), ParaEQParams::create())
{
    using namespace ParaEQParams;

    for (int i = 0; i < kNumBands; ++i)
    {
        const auto& b = bands[i];
        auto& p = params[(size_t) i];
        p.on    = findParam (id (b, "on"));
        p.type  = findParam (id (b, "type"));
        p.freq  = findParam (id (b, "freq"));
        p.gain  = findParam (id (b, "gain"));
        p.q     = findParam (id (b, "q"));
        p.slope = findParam (id (b, "slope"));
    }
}

ParaEQEngine::Settings RKParaEQProcessor::currentSettings() const
{
    ParaEQEngine::Settings s;

    const auto cut = [this] (int i)
    {
        const auto& p = params[(size_t) i];
        return ParaEQEngine::Cut { valueOf (p.on) >= 0.5f, valueOf (p.freq), juce::roundToInt (valueOf (p.slope)) + 1 };
    };

    s.highPass = cut (0);
    s.lowPass = cut (ParaEQParams::kNumBands - 1);

    for (int b = 0; b < ParaEQEngine::kBands; ++b)
    {
        const auto& p = params[(size_t) b + 1];
        auto& band = s.bands[(size_t) b];
        band.on = valueOf (p.on) >= 0.5f;
        band.bell = p.type == nullptr || valueOf (p.type) >= 0.5f;
        band.hz = valueOf (p.freq);
        band.gainDb = valueOf (p.gain);
        band.q = valueOf (p.q);
    }

    return s;
}

void RKParaEQProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKParaEQProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKParaEQProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKParaEQProcessor::createEditor()
{
    return new RKParaEQEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKParaEQProcessor();
}
#endif
