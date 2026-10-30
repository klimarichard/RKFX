#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKTrigger";
        c.stateVersion = 1;
        c.hasMix = false;                     // the audio passes through (or is muted)
        c.hasOutput = false;
        c.midiOutput = true;
        c.accent = juce::Colour (0xff9e3c7a); // MIDI family, deep orchid
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKTriggerProcessor::RKTriggerProcessor()
    : rkfx::ProcessorBase (makeConfig(), TriggerParams::create()),
      mode (findParam ("mode")), crosstalk (findParam ("crosstalk")), audio (findParam ("audio"))
{
    using TriggerParams::id;
    for (int t = 0; t < TriggerParams::kTriggers; ++t)
        triggerParams[(size_t) t] = { findParam (id (t, "threshold")), findParam (id (t, "hpf")), findParam (id (t, "lpf")),
                                      findParam (id (t, "scan")), findParam (id (t, "mask")), findParam (id (t, "retrigger")),
                                      findParam (id (t, "dynamics")), findParam (id (t, "curve")), findParam (id (t, "vel_min")),
                                      findParam (id (t, "vel_max")), findParam (id (t, "note")), findParam (id (t, "channel")),
                                      findParam (id (t, "length")) };
}

TriggerEngine::Settings RKTriggerProcessor::currentSettings() const
{
    TriggerEngine::Settings s;
    s.mode = valueOf (mode) >= 0.5f ? TriggerEngine::Mode::dual : TriggerEngine::Mode::mono;
    s.crosstalk = valueOf (crosstalk) / 100.0f;
    s.mute = valueOf (audio) >= 0.5f;

    for (size_t t = 0; t < triggerParams.size(); ++t)
    {
        const auto& p = triggerParams[t];
        auto& tr = s.triggers[t];
        tr.thresholdDb = valueOf (p.threshold);
        tr.highPassHz  = valueOf (p.hpf);
        tr.lowPassHz   = valueOf (p.lpf);
        tr.scanMs      = valueOf (p.scan);
        tr.maskMs      = valueOf (p.mask);
        tr.retriggerDb = valueOf (p.retrigger) < 2.5f ? 0.0f : valueOf (p.retrigger);
        tr.dynamicsDb  = valueOf (p.dynamics);
        tr.curve       = valueOf (p.curve) / 100.0f;
        tr.velocityMin = juce::roundToInt (valueOf (p.velMin));
        tr.velocityMax = juce::roundToInt (valueOf (p.velMax));
        tr.note        = juce::roundToInt (valueOf (p.note));
        tr.channel     = juce::roundToInt (valueOf (p.channel));
        tr.lengthMs    = valueOf (p.length);
    }
    return s;
}

TriggerEngine::Meter RKTriggerProcessor::takeMeter (int trigger) noexcept
{
    auto& m = meters[(size_t) trigger];
    return { m.levelDb.exchange (rkfx::Dynamics::kSilenceDb), m.hits.load(), m.velocity.load() };
}

void RKTriggerProcessor::prepareDsp (double sampleRate, int, int)
{
    scratch.ensureSize (8192);
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKTriggerProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

rkfx::MidiFootprint RKTriggerProcessor::getMidiOutputFootprint() const
{
    rkfx::MidiFootprint f;
    const auto dual = juce::roundToInt (valueOf (findParam ("mode"))) == 1;
    for (int t = 0; t < (dual ? TriggerParams::kTriggers : 1); ++t)
        f.addNote (juce::roundToInt (valueOf (triggerParams[(size_t) t].channel)),
                   juce::roundToInt (valueOf (triggerParams[(size_t) t].note)));
    return f;
}

void RKTriggerProcessor::releaseMidi (juce::MidiBuffer& midi)
{
    engine.release (midi, 0);
}

void RKTriggerProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());
    scratch.clear();
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), scratch);
    for (const auto m : scratch)
        sendMidi (m.getMessage(), m.samplePosition);

    for (int t = 0; t < TriggerParams::kTriggers; ++t)
    {
        const auto m = engine.takeMeter (t);
        auto& target = meters[(size_t) t];
        auto current = target.levelDb.load();
        while (m.levelDb > current && ! target.levelDb.compare_exchange_weak (current, m.levelDb)) {}
        target.hits = m.hits;
        target.velocity = m.velocity;
    }
}

juce::AudioProcessorEditor* RKTriggerProcessor::createEditor()
{
    return new RKTriggerEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKTriggerProcessor();
}
#endif
