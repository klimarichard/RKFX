#include "TriggerEngine.h"

#include <cmath>

using rkfx::Dynamics::kSilenceDb;
using rkfx::Dynamics::toDb;

namespace
{
    int msToSamples (float ms, double rate) noexcept { return juce::jmax (1, (int) std::lround (ms * 0.001 * rate)); }
    float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
}

int TriggerEngine::velocityFor (const Trigger& t, float peakDb)
{
    const auto lo = juce::jlimit (1, 127, juce::jmin (t.velocityMin, t.velocityMax));
    const auto hi = juce::jlimit (1, 127, juce::jmax (t.velocityMin, t.velocityMax));
    const auto x = juce::jlimit (0.0f, 1.0f, (peakDb - t.thresholdDb) / juce::jmax (1.0f, t.dynamicsDb));

    // Curve: +100% rises fast at first (exponent 1/4), -100% slowly (exponent 4).
    const auto shaped = std::pow (x, std::pow (4.0f, -juce::jlimit (-1.0f, 1.0f, t.curve)));
    return juce::jlimit (1, 127, juce::roundToInt ((float) lo + shaped * (float) (hi - lo)));
}

void TriggerEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    envelopeCoef = rkfx::Dynamics::timeCoefficient (kMaskEnvelopeMs, sampleRate);
    attackCoef = rkfx::Dynamics::timeCoefficient (kReferenceAttackMs, sampleRate);
    for (auto& s : states)
    {
        s.highPass.prepare (sampleRate);
        s.lowPass.prepare (sampleRate);
    }
    audioGain.reset (sampleRate, rkfx::kBypassFadeSeconds);
    setSettings (settings);
    reset();
}

void TriggerEngine::reset()
{
    for (auto& s : states)
    {
        s.highPass.reset();
        s.lowPass.reset();
        s.scanLeft = s.maskLeft = s.noteLeft = 0;
        s.scanPeak = s.envelope = s.hitPeak = 0.0f;
        s.sinceHit = 1 << 20;
        s.suppressed = false;
        s.soundingNote = -1;
        s.meter = {};
    }
    audioGain.setCurrentAndTargetValue (audioGain.getTargetValue());
}

void TriggerEngine::setSettings (const Settings& s)
{
    if (s.mode != settings.mode)
        releasePending = true;
    for (int i = 0; i < kTriggers; ++i)
        if (s.triggers[(size_t) i].note != settings.triggers[(size_t) i].note
            || s.triggers[(size_t) i].channel != settings.triggers[(size_t) i].channel)
            releasePending = true;

    settings = s;
    audioGain.setTargetValue (s.mute ? 0.0f : 1.0f);

    for (int i = 0; i < kTriggers; ++i)
    {
        const auto& t = s.triggers[(size_t) i];
        auto& st = states[(size_t) i];
        if (t.highPassHz > 0.0f) st.highPass.setCutoff (t.highPassHz);
        if (t.lowPassHz > 0.0f)  st.lowPass.setCutoff (t.lowPassHz);
    }
}

TriggerEngine::Meter TriggerEngine::takeMeter (int trigger) noexcept
{
    auto& m = states[(size_t) trigger].meter;
    const auto result = m;
    m.levelDb = kSilenceDb;
    return result;
}

void TriggerEngine::release (juce::MidiBuffer& midi, int sample) noexcept
{
    for (auto& s : states)
    {
        if (s.soundingNote >= 0)
            midi.addEvent (juce::MidiMessage::noteOff (s.soundingChannel, s.soundingNote), sample);
        s.soundingNote = -1;
        s.noteLeft = 0;
        s.scanLeft = 0;
    }
}

void TriggerEngine::emit (int index, juce::MidiBuffer& midi, int sample) noexcept
{
    auto& s = states[(size_t) index];
    const auto& t = settings.triggers[(size_t) index];

    // Masked from the start of the hit, whether or not it is sent.
    s.maskLeft = juce::jmax (0, msToSamples (t.maskMs, sampleRate) - msToSamples (t.scanMs, sampleRate));

    // From here the reference level starts at the hit's peak, so its own ring stays under it.
    s.envelope = juce::jmax (s.envelope, s.scanPeak);

    // Crosstalk: a much weaker hit right after the other trigger's is its echo.
    if (settings.mode == Mode::dual)
    {
        auto& other = states[(size_t) (1 - index)];
        const auto window = msToSamples (kCrosstalkMs, sampleRate);
        if (other.sinceHit <= window && s.scanPeak < settings.crosstalk * other.hitPeak)
            s.suppressed = true;
        if (other.scanLeft > 0 && other.scanPeak < settings.crosstalk * s.scanPeak)
            other.suppressed = true;
    }

    if (s.suppressed)
    {
        s.suppressed = false;
        return;
    }

    const auto note = juce::jlimit (0, 127, t.note);
    const auto channel = juce::jlimit (1, 16, t.channel);
    const auto velocity = velocityFor (t, toDb (s.scanPeak));

    if (s.soundingNote >= 0)
        midi.addEvent (juce::MidiMessage::noteOff (s.soundingChannel, s.soundingNote), sample);
    midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity), sample);

    s.soundingNote = note;
    s.soundingChannel = channel;
    s.noteLeft = msToSamples (t.lengthMs, sampleRate);
    s.sinceHit = 0;
    s.hitPeak = s.scanPeak;
    ++s.meter.hits;
    s.meter.velocity = velocity;
}

void TriggerEngine::detect (int index, float x, juce::MidiBuffer& midi, int sample) noexcept
{
    auto& s = states[(size_t) index];
    const auto& t = settings.triggers[(size_t) index];
    using Type = rkfx::StateVariableFilter::Type;

    if (t.highPassHz > 0.0f) x = s.highPass.process (x, Type::highpass);
    if (t.lowPassHz > 0.0f)  x = s.lowPass.process (x, Type::lowpass);
    const auto y = std::abs (x);

    s.meter.levelDb = juce::jmax (s.meter.levelDb, toDb (y));
    s.sinceHit = juce::jmin (s.sinceHit + 1, 1 << 20);

    // The note's end.
    if (s.soundingNote >= 0 && --s.noteLeft <= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (s.soundingChannel, s.soundingNote), sample);
        s.soundingNote = -1;
    }

    // The reference level before this sample, then updated.
    const auto reference = s.envelope;
    s.envelope = y + (y > s.envelope ? attackCoef : envelopeCoef) * (s.envelope - y);

    if (s.scanLeft > 0)
    {
        s.scanPeak = juce::jmax (s.scanPeak, y);
        if (--s.scanLeft == 0)
            emit (index, midi, sample);
        return;
    }

    if (s.maskLeft > 0)
        --s.maskLeft;

    if (y <= dbToGain (t.thresholdDb))
        return;

    const auto free = s.maskLeft <= 0 && y > reference * dbToGain (kRearmDb);
    const auto louder = s.maskLeft > 0 && t.retriggerDb > 0.0f && y > reference * dbToGain (t.retriggerDb);
    if (free || louder)
    {
        s.scanLeft = msToSamples (t.scanMs, sampleRate);
        s.scanPeak = y;
        s.suppressed = false;
    }
}

void TriggerEngine::process (float* const* io, int numChannels, int numSamples, juce::MidiBuffer& midi) noexcept
{
    numChannels = juce::jmax (1, numChannels);
    const auto dual = settings.mode == Mode::dual && numChannels >= 2;

    if (releasePending)
    {
        release (midi, 0);
        releasePending = false;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        if (dual)
        {
            detect (0, io[0][i], midi, i);
            detect (1, io[1][i], midi, i);
        }
        else
        {
            auto x = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                x += io[ch][i];
            detect (0, x / (float) numChannels, midi, i);
        }

        const auto g = audioGain.getNextValue();
        if (g < 1.0f)
            for (int ch = 0; ch < numChannels; ++ch)
                io[ch][i] *= g;
    }
}
