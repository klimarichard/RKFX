#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKMultiComp";
        c.stateVersion = 1;
        c.hasMix = true;                      // parallel compression
        c.mixDefault = 100.0f;
        c.mixInDsp = true;                    // the dry signal gets the crossovers' phase shift
        c.accent = juce::Colour (0xffb8860b); // Dynamics family, dark gold
        c.family = rkfx::Palette::Family::dynamics;
        return c;
    }

    /** Keeps the extreme of the old and new value (lock-free). */
    template <typename Better>
    void accumulate (std::atomic<float>& target, float value, Better better)
    {
        auto current = target.load();
        while (better (value, current) && ! target.compare_exchange_weak (current, value)) {}
    }
}

RKMultiCompProcessor::RKMultiCompProcessor()
    : rkfx::ProcessorBase (makeConfig(), MultiCompParams::create()),
      xoverLow (findParam ("xover_low")), xoverMid (findParam ("xover_mid")), xoverHigh (findParam ("xover_high")),
      bands (findParam ("bands")), offset (findParam ("offset")), knee (findParam ("knee")),
      detect (findParam ("detect")), releaseAuto (findParam ("release_auto")), link (findParam ("link")),
      gainAuto (findParam ("gain_auto")), mix (findParam ("mix"))
{
    using MultiCompParams::id;

    for (const auto& b : MultiCompParams::bands)
        bandParams[(size_t) b.band] = { findParam (id (b, "threshold")), findParam (id (b, "ratio")),
                                        findParam (id (b, "attack")), findParam (id (b, "release")),
                                        findParam (id (b, "gain")), findParam (id (b, "solo")),
                                        findParam (id (b, "on")) };
}

MultiCompEngine::Settings RKMultiCompProcessor::currentSettings() const
{
    MultiCompEngine::Settings s;

    for (size_t i = 0; i < bandParams.size(); ++i)
    {
        const auto& p = bandParams[i];
        auto& b = s.bands[i];
        b.thresholdDb = valueOf (p.threshold);
        b.ratio       = valueOf (p.ratio);
        b.attackMs    = valueOf (p.attack);
        b.releaseMs   = valueOf (p.release);
        b.gainDb      = valueOf (p.gain);
        b.solo        = valueOf (p.solo) >= 0.5f;
        b.on          = valueOf (p.on) >= 0.5f;
    }

    s.lowHz       = valueOf (xoverLow);
    s.midHz       = valueOf (xoverMid);
    s.highHz      = valueOf (xoverHigh);
    s.fourBands   = valueOf (bands) >= 0.5f;
    s.offsetDb    = valueOf (offset);
    s.kneeDb      = valueOf (knee);
    s.detection   = valueOf (detect) >= 0.5f ? rkfx::Dynamics::LevelDetector::Mode::rms
                                             : rkfx::Dynamics::LevelDetector::Mode::peak;
    s.autoRelease = valueOf (releaseAuto) >= 0.5f;
    s.link        = valueOf (link) / 100.0f;
    s.autoGain    = valueOf (gainAuto) >= 0.5f;
    s.mix         = valueOf (mix) / 100.0f;
    return s;
}

MultiCompEngine::Meter RKMultiCompProcessor::takeMeter (MultiCompEngine::Band band) noexcept
{
    auto& m = meters[(size_t) band];
    return { m.levelDb.exchange (rkfx::Dynamics::kSilenceDb), m.reductionDb.exchange (0.0f) };
}

void RKMultiCompProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKMultiCompProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKMultiCompProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    for (const auto& b : MultiCompParams::bands)
    {
        const auto m = engine.takeMeter (b.band);
        auto& target = meters[(size_t) b.band];
        accumulate (target.levelDb, m.levelDb, [] (float x, float y) { return x > y; });
        accumulate (target.reductionDb, m.reductionDb, [] (float x, float y) { return x < y; });
    }
}

juce::AudioProcessorEditor* RKMultiCompProcessor::createEditor()
{
    return new RKMultiCompEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKMultiCompProcessor();
}
#endif
