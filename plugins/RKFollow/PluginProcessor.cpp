#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKFollow";
        c.stateVersion = 1;
        c.hasMix = false;                     // the audio passes through untouched
        c.hasOutput = false;
        c.midiOutput = true;
        c.accent = juce::Colour (0xffcc6ba8); // MIDI family, orchid
        c.family = rkfx::Palette::Family::midi;
        return c;
    }
}

RKFollowProcessor::RKFollowProcessor()
    : rkfx::ProcessorBase (makeConfig(), FollowParams::create()),
      notes (findParam ("notes")), range (findParam ("range")), threshold (findParam ("threshold")),
      transpose (findParam ("transpose")), velMin (findParam ("vel_min")), velMax (findParam ("vel_max")),
      retrigger (findParam ("retrigger")), bend (findParam ("bend")), bendRange (findParam ("bend_range")),
      glide (findParam ("glide")), channel (findParam ("channel")), expr (findParam ("expr")),
      exprCc (findParam ("expr_cc")), exprFloor (findParam ("expr_floor")), exprCeiling (findParam ("expr_ceiling")),
      exprMin (findParam ("expr_min")), exprMax (findParam ("expr_max")), exprCurve (findParam ("expr_curve")),
      exprResponse (findParam ("expr_response"))
{
}

FollowEngine::Settings RKFollowProcessor::currentSettings() const
{
    using Range = rkfx::PitchTracker::Range;
    constexpr Range ranges[] = { Range::violin, Range::voice, Range::guitar, Range::bass };

    FollowEngine::Settings s;
    s.notes         = valueOf (notes) >= 0.5f;
    s.range         = ranges[juce::jlimit (0, 3, juce::roundToInt (valueOf (range)))];
    s.thresholdDb   = valueOf (threshold);
    s.transpose     = juce::roundToInt (valueOf (transpose));
    s.velocityMin   = juce::roundToInt (valueOf (velMin));
    s.velocityMax   = juce::roundToInt (valueOf (velMax));
    s.retriggerDb   = valueOf (retrigger) < 5.5f ? 0.0f : valueOf (retrigger);
    s.bend          = (FollowEngine::Bend) juce::jlimit (0, 2, juce::roundToInt (valueOf (bend)));
    s.bendRange     = FollowParams::bendRangeSemitones (juce::roundToInt (valueOf (bendRange)));
    s.glideMs       = valueOf (glide);
    s.channel       = juce::roundToInt (valueOf (channel));
    s.expression    = valueOf (expr) >= 0.5f;
    s.controller    = juce::roundToInt (valueOf (exprCc));
    s.floorDb       = valueOf (exprFloor);
    s.ceilingDb     = valueOf (exprCeiling);
    s.expressionMin = juce::roundToInt (valueOf (exprMin));
    s.expressionMax = juce::roundToInt (valueOf (exprMax));
    s.curve         = valueOf (exprCurve) / 100.0f;
    s.responseMs    = valueOf (exprResponse);
    return s;
}

FollowEngine::Meter RKFollowProcessor::takeMeter() noexcept
{
    FollowEngine::Meter m;
    m.note = meterNote.load();
    m.bendCents = meterBend.load();
    m.velocity = meterVelocity.load();
    m.levelDb = meterLevel.exchange (rkfx::Dynamics::kSilenceDb);
    m.expression = meterExpression.load();
    return m;
}

void RKFollowProcessor::prepareDsp (double sampleRate, int, int)
{
    scratch.ensureSize (8192);
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKFollowProcessor::resetDsp()
{
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKFollowProcessor::releaseMidi (juce::MidiBuffer& midi)
{
    engine.release (midi, 0);
    meterNote = -1;
}

void RKFollowProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    // The audio isn't touched: only listened to.
    engine.setSettings (currentSettings());
    scratch.clear();
    engine.process (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples(), scratch);
    for (const auto m : scratch)
        sendMidi (m.getMessage(), m.samplePosition);

    const auto m = engine.takeMeter();
    meterNote = m.note;
    meterBend = m.bendCents;
    meterVelocity = m.velocity;
    meterExpression = m.expression;
    auto current = meterLevel.load();
    while (m.levelDb > current && ! meterLevel.compare_exchange_weak (current, m.levelDb)) {}
}

juce::AudioProcessorEditor* RKFollowProcessor::createEditor()
{
    return new RKFollowEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKFollowProcessor();
}
#endif
