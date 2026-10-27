#include "GateEngine.h"

#include <cmath>

using rkfx::Dynamics::toDb;
using rkfx::Dynamics::toGain;

namespace
{
    // Detector: instant rise, 10 ms fall: smooth enough not to dip at every
    // zero crossing of a low note (hold and hysteresis cover the rest), fast
    // enough to see a note end within about 50 ms.
    constexpr float kDetectorAttackMs = 0.0f;
    constexpr float kDetectorReleaseMs = 10.0f;
}

float GateEngine::targetGainDb (const Settings& s, float levelDb, bool& open)
{
    const auto floor = s.rangeDb <= kClosedDb ? kFloorDb : juce::jmin (0.0f, s.rangeDb);

    if (s.mode == Mode::gate)
    {
        // Opens at the threshold, closes only below threshold - hysteresis.
        if (levelDb >= s.thresholdDb)
            open = true;
        else if (levelDb < s.thresholdDb - s.hysteresisDb)
            open = false;

        return open ? 0.0f : floor;
    }

    // Expander: every dB below the threshold becomes `ratio` dB, limited by the range.
    open = levelDb >= s.thresholdDb;
    if (open)
        return 0.0f;

    return juce::jmax (floor, (levelDb - s.thresholdDb) * (s.ratio - 1.0f));
}

void GateEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& c : channels)
    {
        c.envelope.prepare (sampleRate);
        c.envelope.setTimes (kDetectorAttackMs, kDetectorReleaseMs);
        c.highPass.prepare (sampleRate);
        c.lowPass.prepare (sampleRate);
        c.smoother.prepare (sampleRate);
    }

    listenMix.reset (sampleRate, rkfx::kBypassFadeSeconds);
    setSettings (settings);
    reset();
}

void GateEngine::reset()
{
    for (auto& c : channels)
    {
        c.envelope.reset();
        c.highPass.reset();
        c.lowPass.reset();
        c.smoother.reset();
        c.open = false;
        c.holdRemaining = 0;
    }

    listenMix.setCurrentAndTargetValue (listenMix.getTargetValue());
    peakReduction = 0.0f;
}

void GateEngine::setSettings (const Settings& s)
{
    settings = s;
    holdSamples = (int) (s.holdMs * 0.001f * (float) sampleRate);
    listenMix.setTargetValue (s.listen ? 1.0f : 0.0f);

    for (auto& c : channels)
    {
        // The smoother's "attack" is for falling gain: that is the gate's release.
        c.smoother.setTimes (s.releaseMs, s.attackMs);
        c.smoother.setAutoRelease (false);
    }
}

float GateEngine::takeGainReductionDb() noexcept
{
    const auto r = peakReduction;
    peakReduction = 0.0f;
    return r;
}

float GateEngine::gateChannel (Channel& c, float levelDb) noexcept
{
    const auto wasOpen = c.open;
    auto target = targetGainDb (settings, levelDb, c.open);

    // Hold: while open the countdown is refreshed; once the gate wants to
    // close, it stays fully open (and counts as open) until the hold runs out.
    if (c.open)
    {
        c.holdRemaining = holdSamples;
    }
    else if (wasOpen && c.holdRemaining > 0)
    {
        --c.holdRemaining;
        target = 0.0f;
        c.open = true;
    }

    return c.smoother.process (target);
}

void GateEngine::process (float* const* io, int numChannels, int numSamples,
                          const float* const* sidechain, int sidechainChannels) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    const auto highPassOn = settings.keyHighPassHz > 0.0f;
    const auto lowPassOn = settings.keyLowPassHz > 0.0f;

    for (auto& c : channels)
    {
        if (highPassOn && ! highPassRunning) c.highPass.reset();
        if (lowPassOn && ! lowPassRunning)   c.lowPass.reset();
        if (highPassOn) c.highPass.setCutoff (settings.keyHighPassHz);
        if (lowPassOn)  c.lowPass.setCutoff (settings.keyLowPassHz);
    }
    highPassRunning = highPassOn;
    lowPassRunning = lowPassOn;

    const auto closedFully = settings.rangeDb <= kClosedDb;

    for (int i = 0; i < numSamples; ++i)
    {
        float key[kMaxChannels] {}, levelDb[kMaxChannels] {};
        float loudest = kFloorDb;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];

            float d = 0.0f;
            if (! settings.external)
                d = io[ch][i];
            else if (sidechain != nullptr && sidechainChannels > 0)
                d = sidechain[juce::jmin (ch, sidechainChannels - 1)][i];

            if (highPassOn) d = c.highPass.process (d, rkfx::StateVariableFilter::Type::highpass);
            if (lowPassOn)  d = c.lowPass.process (d, rkfx::StateVariableFilter::Type::lowpass);

            key[ch] = d;
            levelDb[ch] = toDb (c.envelope.process (d));
            loudest = juce::jmax (loudest, levelDb[ch]);
        }

        // Linked: one decision, from the louder channel, for both.
        float linkedGainDb = 0.0f;
        if (settings.link)
            linkedGainDb = gateChannel (channels[0], loudest);

        const auto listen = listenMix.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto gainDb = settings.link ? linkedGainDb : gateChannel (channels[(size_t) ch], levelDb[ch]);
            peakReduction = juce::jmin (peakReduction, gainDb);

            const auto gain = (closedFully && gainDb <= kClosedDb) ? 0.0f : toGain (gainDb);
            auto y = io[ch][i] * gain;
            if (listen > 0.0f)
                y += listen * (key[ch] - y);
            io[ch][i] = y;
        }
    }
}
