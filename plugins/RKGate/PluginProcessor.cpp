#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKGate";
        c.stateVersion = 1;
        c.hasMix = false;
        c.sidechain = true;
        c.accent = juce::Colour (0xffd9a21b); // Dynamics family, amber yellow
        c.family = rkfx::Palette::Family::dynamics;
        return c;
    }
}

RKGateProcessor::RKGateProcessor()
    : rkfx::ProcessorBase (makeConfig(), GateParams::create()),
      mode (findParam ("mode")), threshold (findParam ("threshold")), ratio (findParam ("ratio")),
      range (findParam ("range")), attack (findParam ("attack")), hold (findParam ("hold")),
      release (findParam ("release")), hysteresis (findParam ("hysteresis")), scSource (findParam ("sc_source")),
      keyHpf (findParam ("key_hpf")), keyLpf (findParam ("key_lpf")), keyListen (findParam ("key_listen")),
      link (findParam ("link"))
{
}

GateEngine::Settings RKGateProcessor::currentSettings() const
{
    GateEngine::Settings s;
    s.mode          = valueOf (mode) >= 0.5f ? GateEngine::Mode::expander : GateEngine::Mode::gate;
    s.thresholdDb   = valueOf (threshold);
    s.ratio         = valueOf (ratio);
    s.rangeDb       = valueOf (range);
    s.attackMs      = valueOf (attack);
    s.holdMs        = valueOf (hold);
    s.releaseMs     = valueOf (release);
    s.hysteresisDb  = valueOf (hysteresis);
    s.external      = valueOf (scSource) >= 0.5f;
    s.keyHighPassHz = valueOf (keyHpf);
    s.keyLowPassHz  = valueOf (keyLpf);
    s.listen        = valueOf (keyListen) >= 0.5f;
    s.link          = valueOf (link) >= 0.5f;
    return s;
}

void RKGateProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKGateProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKGateProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());

    const auto* sc = getSidechain();
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(),
                    sc != nullptr ? sc->getArrayOfReadPointers() : nullptr,
                    sc != nullptr ? sc->getNumChannels() : 0);

    // Keep the deepest reduction until the meter reads it.
    const auto reduction = engine.takeGainReductionDb();
    auto current = meterReduction.load();
    while (reduction < current && ! meterReduction.compare_exchange_weak (current, reduction)) {}
}

juce::AudioProcessorEditor* RKGateProcessor::createEditor()
{
    return new RKGateEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKGateProcessor();
}
