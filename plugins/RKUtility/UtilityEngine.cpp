#include "UtilityEngine.h"

#include <cmath>

std::array<float, 4> UtilityEngine::matrixFor (const Settings& s) noexcept
{
    std::array<float, 4> m;
    switch (s.channelMode)
    {
        case ChannelMode::stereo:        m = { 1.0f, 0.0f, 0.0f, 1.0f }; break;
        case ChannelMode::leftOnly:      m = { 1.0f, 0.0f, 1.0f, 0.0f }; break;
        case ChannelMode::rightOnly:     m = { 0.0f, 1.0f, 0.0f, 1.0f }; break;
        case ChannelMode::midSideEncode: m = { 0.5f, 0.5f, 0.5f, -0.5f }; break;
        case ChannelMode::midSideDecode: m = { 1.0f, 1.0f, 1.0f, -1.0f }; break;
    }

    if (s.swap)
        m = { m[2], m[3], m[0], m[1] };

    if (s.invertLeft)  { m[0] = -m[0]; m[1] = -m[1]; }
    if (s.invertRight) { m[2] = -m[2]; m[3] = -m[3]; }
    return m;
}

void UtilityEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& d : delays)
        d.prepare ((int) std::ceil (kMaxDelayMs * 0.001 * sampleRate) + 8);
    for (auto& x : crossovers)
        x.prepare (sampleRate);

    for (auto& m : matrix)
        m.reset (sampleRate, rkfx::kBypassFadeSeconds);
    for (auto* s : { &delayLeft, &delayRight, &width, &gainLeft, &gainRight, &level })
        s->reset (sampleRate, rkfx::kParamSmoothingSeconds);
    muteGain.reset (sampleRate, rkfx::kBypassFadeSeconds);
    bassMonoMix.reset (sampleRate, rkfx::kBypassFadeSeconds);

    setSettings (settings);
    reset();
}

void UtilityEngine::reset()
{
    for (auto& d : delays)
        d.reset();
    for (auto& x : crossovers)
        x.reset();
    bassMonoRunning = settings.bassMonoHz > 0.0f;

    for (auto& m : matrix)
        m.setCurrentAndTargetValue (m.getTargetValue());
    for (auto* s : { &delayLeft, &delayRight, &width, &gainLeft, &gainRight, &level, &muteGain, &bassMonoMix })
        s->setCurrentAndTargetValue (s->getTargetValue());
}

void UtilityEngine::setSettings (const Settings& s)
{
    settings = s;

    const auto m = matrixFor (s);
    for (size_t i = 0; i < 4; ++i)
        matrix[i].setTargetValue (m[i]);

    const auto msToSamples = (float) (0.001 * sampleRate);
    delayLeft.setTargetValue (juce::jlimit (0.0f, kMaxDelayMs, s.delayLeftMs) * msToSamples);
    delayRight.setTargetValue (juce::jlimit (0.0f, kMaxDelayMs, s.delayRightMs) * msToSamples);
    width.setTargetValue (juce::jlimit (0.0f, 2.0f, s.width));

    // Balance: centre is unity on both sides; turning towards one side fades the other.
    const auto pan = juce::jlimit (-1.0f, 1.0f, s.pan);
    gainLeft.setTargetValue (juce::jmin (1.0f, 1.0f - pan));
    gainRight.setTargetValue (juce::jmin (1.0f, 1.0f + pan));

    level.setTargetValue (s.gainDb <= kSilentDb ? 0.0f : juce::Decibels::decibelsToGain (juce::jmin (24.0f, s.gainDb)));
    muteGain.setTargetValue (s.mute ? 0.0f : 1.0f);
    bassMonoMix.setTargetValue (s.bassMonoHz > 0.0f ? 1.0f : 0.0f);

    if (s.bassMonoHz > 0.0f)
        for (auto& x : crossovers)
            x.setFrequency (juce::jlimit (20.0f, 1000.0f, s.bassMonoHz));
}

void UtilityEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    const auto stereo = numChannels >= 2;

    const auto smoothingAny = [] (std::initializer_list<const rkfx::Smoothed*> list)
    {
        for (auto* s : list)
            if (s->isSmoothing())
                return true;
        return false;
    };

    // Which stages can be skipped this block (neutral and not gliding).
    const auto matrixIdentity = ! smoothingAny ({ &matrix[0], &matrix[1], &matrix[2], &matrix[3] })
                              && matrix[0].getTargetValue() >= 1.0f && matrix[3].getTargetValue() >= 1.0f
                              && std::abs (matrix[1].getTargetValue()) < 1.0e-9f && std::abs (matrix[2].getTargetValue()) < 1.0e-9f;
    const auto delaysOff = ! smoothingAny ({ &delayLeft, &delayRight })
                         && delayLeft.getTargetValue() <= 0.0f && delayRight.getTargetValue() <= 0.0f;
    const auto widthNeutral = ! width.isSmoothing() && std::abs (width.getTargetValue() - 1.0f) < 1.0e-9f;

    const auto bassMonoNeeded = settings.bassMonoHz > 0.0f || bassMonoMix.isSmoothing();
    if (bassMonoNeeded && ! bassMonoRunning)
        for (auto& x : crossovers)
            x.reset();
    bassMonoRunning = bassMonoNeeded;

    for (int i = 0; i < numSamples; ++i)
    {
        auto l = io[0][i];
        auto r = stereo ? io[1][i] : l;

        // Channel mode, swap and polarity.
        const auto m0 = matrix[0].getNextValue(), m1 = matrix[1].getNextValue();
        const auto m2 = matrix[2].getNextValue(), m3 = matrix[3].getNextValue();
        if (! matrixIdentity)
        {
            const auto nl = m0 * l + m1 * r;
            const auto nr = m2 * l + m3 * r;
            l = nl;
            r = nr;
        }

        // Channel delay (time alignment). The lines always run, so turning it on is seamless.
        const auto dl = delayLeft.getNextValue(), dr = delayRight.getNextValue();
        delays[0].write (l);
        delays[1].write (r);
        if (! delaysOff)
        {
            if (dl > 0.0f) l = delays[0].read (dl);
            if (dr > 0.0f) r = delays[1].read (dr);
        }

        // Width (mid/side).
        const auto w = width.getNextValue();
        if (stereo && ! widthNeutral)
        {
            const auto mid = 0.5f * (l + r);
            const auto side = 0.5f * (l - r) * w;
            l = mid + side;
            r = mid - side;
        }

        // Bass mono: the lows of both sides summed to mono, the highs untouched.
        const auto bm = bassMonoMix.getNextValue();
        if (stereo && bassMonoRunning)
        {
            float lowL, highL, lowR, highR;
            crossovers[0].process (l, lowL, highL);
            crossovers[1].process (r, lowR, highR);
            const auto monoLow = 0.5f * (lowL + lowR);
            l += bm * ((monoLow + highL) - l);
            r += bm * ((monoLow + highR) - r);
        }

        // Balance, gain, mute.
        const auto g = level.getNextValue() * muteGain.getNextValue();
        l *= gainLeft.getNextValue() * g;
        r *= gainRight.getNextValue() * g;

        io[0][i] = l;
        if (stereo)
            io[1][i] = r;
    }
}
