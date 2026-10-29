#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKRide";
        c.stateVersion = 1;
        c.hasMix = false;                     // blending only weakens the riding (Max Boost / Max Cut do that)
        c.sidechain = true;                   // the music, for Relative mode
        c.accent = juce::Colour (0xffe3b450); // Dynamics family, honey
        c.family = rkfx::Palette::Family::dynamics;
        return c;
    }
}

RKRideProcessor::RKRideProcessor()
    : rkfx::ProcessorBase (makeConfig(), RideParams::create()),
      target (findParam ("target")), maxBoost (findParam ("max_boost")), maxCut (findParam ("max_cut")),
      speed (findParam ("speed")), floor (findParam ("floor")), returnAfter (findParam ("return")),
      mode (findParam ("mode")), above (findParam ("above"))
{
}

RideEngine::Settings RKRideProcessor::currentSettings() const
{
    RideEngine::Settings s;
    s.targetDb   = valueOf (target);
    s.maxBoostDb = valueOf (maxBoost);
    s.maxCutDb   = valueOf (maxCut);
    s.speedMs    = valueOf (speed);
    s.floorDb    = valueOf (floor);
    s.returnMs   = valueOf (returnAfter);
    s.relative   = valueOf (mode) >= 0.5f;
    s.aboveDb    = valueOf (above);
    return s;
}

RideEngine::Meter RKRideProcessor::takeMeter (bool& fresh) noexcept
{
    fresh = meterFresh.exchange (false);
    RideEngine::Meter m;
    m.levelDb = meterLevel.exchange (rkfx::Dynamics::kSilenceDb);
    m.targetDb = meterTarget.load();
    m.gainDb = meterGain.load();
    m.holding = meterHolding.load();
    return m;
}

void RKRideProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKRideProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKRideProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());

    const auto* sc = getSidechain();
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(),
                    sc != nullptr ? sc->getArrayOfReadPointers() : nullptr,
                    sc != nullptr ? sc->getNumChannels() : 0);

    const auto m = engine.takeMeter();
    auto current = meterLevel.load();
    while (m.levelDb > current && ! meterLevel.compare_exchange_weak (current, m.levelDb)) {}
    meterTarget = m.targetDb;
    meterGain = m.gainDb;
    meterHolding = m.holding;
    meterFresh = true;
}

juce::AudioProcessorEditor* RKRideProcessor::createEditor()
{
    return new RKRideEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKRideProcessor();
}
#endif
