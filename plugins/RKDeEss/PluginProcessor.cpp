#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKDeEss";
        c.stateVersion = 1;
        c.hasMix = false;
        c.accent = juce::Colour (0xfff5d76e); // Dynamics family, pale yellow
        return c;
    }
}

RKDeEssProcessor::RKDeEssProcessor()
    : rkfx::ProcessorBase (makeConfig(), DeEssParams::create()),
      freq (findParam ("freq")), detect (findParam ("detect")), threshold (findParam ("threshold")),
      thresholdRel (findParam ("threshold_rel")), range (findParam ("range")), mode (findParam ("mode")),
      listen (findParam ("listen")), link (findParam ("link"))
{
}

DeEssEngine::Settings RKDeEssProcessor::currentSettings() const
{
    DeEssEngine::Settings s;
    s.frequencyHz         = valueOf (freq);
    s.detection           = valueOf (detect) >= 0.5f ? DeEssEngine::Detection::absolute : DeEssEngine::Detection::relative;
    s.thresholdDb         = valueOf (threshold);
    s.relativeThresholdDb = valueOf (thresholdRel);
    s.rangeDb             = valueOf (range);
    s.mode                = valueOf (mode) >= 0.5f ? DeEssEngine::Mode::wide : DeEssEngine::Mode::split;
    s.listen              = valueOf (listen) >= 0.5f;
    s.link                = valueOf (link) >= 0.5f;
    return s;
}

void RKDeEssProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKDeEssProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKDeEssProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    const auto reduction = engine.takeGainReductionDb();
    auto current = meterReduction.load();
    while (reduction < current && ! meterReduction.compare_exchange_weak (current, reduction)) {}
}

juce::AudioProcessorEditor* RKDeEssProcessor::createEditor()
{
    return new RKDeEssEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKDeEssProcessor();
}
