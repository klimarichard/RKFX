#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKCompress";
        c.stateVersion = 1;
        c.hasMix = true;                      // parallel compression
        c.mixDefault = 100.0f;
        c.sidechain = true;
        c.accent = juce::Colour (0xfff2c230); // Dynamics family, golden yellow
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

RKCompressProcessor::RKCompressProcessor()
    : rkfx::ProcessorBase (makeConfig(), CompressParams::create()),
      threshold (findParam ("threshold")), ratio (findParam ("ratio")), attack (findParam ("attack")),
      release (findParam ("release")), releaseAuto (findParam ("release_auto")), knee (findParam ("knee")),
      detect (findParam ("detect")), scSource (findParam ("sc_source")), scHpf (findParam ("sc_hpf")),
      scListen (findParam ("sc_listen")), link (findParam ("link")), makeup (findParam ("makeup")),
      makeupAuto (findParam ("makeup_auto"))
{
}

CompressorEngine::Settings RKCompressProcessor::currentSettings() const
{
    CompressorEngine::Settings s;
    s.thresholdDb    = valueOf (threshold);
    s.ratio          = valueOf (ratio);
    s.attackMs       = valueOf (attack);
    s.releaseMs      = valueOf (release);
    s.autoRelease    = valueOf (releaseAuto) >= 0.5f;
    s.kneeDb         = valueOf (knee);
    s.detection      = valueOf (detect) >= 0.5f ? rkfx::Dynamics::LevelDetector::Mode::rms
                                                : rkfx::Dynamics::LevelDetector::Mode::peak;
    s.external       = valueOf (scSource) >= 0.5f;
    s.sidechainHpfHz = valueOf (scHpf);
    s.listen         = valueOf (scListen) >= 0.5f;
    s.link           = valueOf (link) / 100.0f;
    s.makeupDb       = valueOf (makeup);
    s.autoMakeup     = valueOf (makeupAuto) >= 0.5f;
    return s;
}

void RKCompressProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKCompressProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKCompressProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());

    const auto* sc = getSidechain();
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(),
                    sc != nullptr ? sc->getArrayOfReadPointers() : nullptr,
                    sc != nullptr ? sc->getNumChannels() : 0);

    accumulate (meterReduction, engine.takeGainReductionDb(), [] (float a, float b) { return a < b; });
    accumulate (meterInput, engine.takeInputLevelDb(), [] (float a, float b) { return a > b; });
}

juce::AudioProcessorEditor* RKCompressProcessor::createEditor()
{
    return new RKCompressEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKCompressProcessor();
}
