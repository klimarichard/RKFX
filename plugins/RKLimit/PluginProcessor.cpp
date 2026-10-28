#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKLimit";
        c.stateVersion = 1;
        c.hasMix = false;                     // a parallel limiter would defeat its protection
        c.accent = juce::Colour (0xffffd54a); // Dynamics family, bright yellow
        c.family = rkfx::Palette::Family::dynamics;
        return c;
    }
}

RKLimitProcessor::RKLimitProcessor()
    : rkfx::ProcessorBase (makeConfig(), LimitParams::create()),
      input (findParam ("input")), ceiling (findParam ("ceiling")), release (findParam ("release")),
      releaseAuto (findParam ("release_auto")), link (findParam ("link")), oversampling (findParam ("oversampling"))
{
    reportLatency (rkfx::Oversampler::latencySamples (currentSettings().oversampling));
}

LimitEngine::Settings RKLimitProcessor::currentSettings() const
{
    LimitEngine::Settings s;
    s.inputDb      = valueOf (input);
    s.ceilingDb    = valueOf (ceiling);
    s.releaseMs    = valueOf (release);
    s.autoRelease  = valueOf (releaseAuto) >= 0.5f;
    s.link         = valueOf (link) / 100.0f;
    s.oversampling = LimitParams::oversamplingFactors[juce::jlimit (0, 2, juce::roundToInt (valueOf (oversampling)))];
    return s;
}

void RKLimitProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
    reportLatency (engine.latencySamples());
}

void RKLimitProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKLimitProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    reportLatency (engine.latencySamples());

    const auto reduction = engine.takeGainReductionDb();
    auto current = meterReduction.load();
    while (reduction < current && ! meterReduction.compare_exchange_weak (current, reduction)) {}
}

juce::AudioProcessorEditor* RKLimitProcessor::createEditor()
{
    return new RKLimitEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKLimitProcessor();
}
#endif
