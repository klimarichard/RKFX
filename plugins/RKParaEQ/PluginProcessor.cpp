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
        p.dyn       = findParam (id (b, "dyn"));
        p.threshold = findParam (id (b, "threshold"));
        p.range     = findParam (id (b, "range"));
        p.ratio     = findParam (id (b, "ratio"));
        p.attack    = findParam (id (b, "attack"));
        p.release   = findParam (id (b, "release"));
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

        auto& d = band.dynamic;
        d.on          = valueOf (p.dyn) >= 0.5f;
        d.thresholdDb = valueOf (p.threshold);
        d.rangeDb     = valueOf (p.range);
        d.ratio       = valueOf (p.ratio);
        d.attackMs    = valueOf (p.attack);
        d.releaseMs   = valueOf (p.release);
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

    for (int b = 0; b < ParaEQEngine::kBands; ++b)
    {
        const auto m = engine.takeMeter (b);
        auto& target = meters[(size_t) b];
        auto current = target.levelDb.load();
        while (m.levelDb > current && ! target.levelDb.compare_exchange_weak (current, m.levelDb)) {}
        target.gainDb = m.gainDb;
    }
}

ParaEQEngine::Meter RKParaEQProcessor::takeMeter (int band) noexcept
{
    auto& m = meters[(size_t) band];
    return { m.levelDb.exchange (rkfx::Dynamics::kSilenceDb), m.gainDb.load() };
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
