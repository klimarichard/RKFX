#include "MixSendEq.h"

namespace
{
    using MixFilters::kSectionFadeSeconds;
    using MixFilters::kCoefficientInterval;

    void setValue (rkfx::Smoothed& v, float target, bool snap) noexcept
    {
        if (snap)
            v.setCurrentAndTargetValue (target);
        else
            v.setTargetValue (target);
    }

    float logOf (float hz) noexcept { return std::log (juce::jlimit (10.0f, 20000.0f, hz)); }
}

void SendEqDsp::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    onFade.amount.reset (sampleRate, kSectionFadeSeconds);
    lowCutFade.amount.reset (sampleRate, kSectionFadeSeconds);
    lowCutLogHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    for (auto& b : bands)
    {
        b.logHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.gainDb.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.q.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    }
    reset();
}

void SendEqDsp::reset()
{
    clearFilters();
    setSettings (settings, true);
}

void SendEqDsp::clearFilters() noexcept
{
    lowCut.clear();
    for (auto& b : bands)
        b.filter.clear();
}

void SendEqDsp::setSettings (const Settings& s, bool snap) noexcept
{
    settings = s;

    // Turning on starts from clean filters, faded in: never a click.
    if (onFade.set (s.on, snap))
        clearFilters();

    // The low cut: Off skips it (faded); turned on, it starts at its frequency.
    const auto cutOn = s.lowCutHz > 0.0f;
    const auto cutWakes = lowCutFade.set (cutOn, snap);
    if (cutOn)
        setValue (lowCutLogHz, logOf (s.lowCutHz), snap || cutWakes);
    if (cutWakes)
        lowCut.clear();
    if (snap || cutWakes)
        updateLowCut();

    const float hz[] = { s.lowHz, s.midHz, s.highHz };
    const float db[] = { s.lowDb, s.midDb, s.highDb };
    const float q[] = { 0.70710678f, s.midQ, 0.70710678f };
    for (int i = 0; i < kBands; ++i)
    {
        auto& b = bands[(size_t) i];
        setValue (b.logHz, logOf (hz[i]), snap);
        setValue (b.gainDb, db[i], snap);
        setValue (b.q, juce::jlimit (0.1f, 20.0f, q[i]), snap);
        if (snap || b.gainDb.isSmoothing())
            updateBand (i);
    }
}

rkfx::BiquadCoefficients SendEqDsp::designBand (int band, double hz, double gainDb, double q, double rate)
{
    using namespace rkfx::MatchedDesign;
    if (band == 0)
        return lowShelf (hz, gainDb, rate);
    if (band == kBands - 1)
        return highShelf (hz, gainDb, rate);
    return bell (hz, gainDb, q, rate);
}

void SendEqDsp::updateBand (int i) noexcept
{
    auto& b = bands[(size_t) i];
    const auto gainDb = b.gainDb.getCurrentValue();
    const auto wasActive = b.active;
    b.active = std::abs (gainDb) >= 1.0e-4f || b.gainDb.isSmoothing();
    if (! b.active)
        return;
    if (! wasActive)
        b.filter.clear();
    b.filter.set (designBand (i, std::exp ((double) b.logHz.getCurrentValue()), gainDb, b.q.getCurrentValue(), sampleRate));
}

void SendEqDsp::updateLowCut() noexcept
{
    lowCut.design (true, std::exp ((double) lowCutLogHz.getCurrentValue()), 1, sampleRate);
}

void SendEqDsp::process (float* const* io, int numSides, int numSamples, juce::AudioBuffer<float>& scratch) noexcept
{
    MixFilters::runSection (onFade, io, numSides, numSamples, scratch, [&]
    {
        for (int start = 0; start < numSamples; start += kCoefficientInterval)
        {
            const auto length = juce::jmin (kCoefficientInterval, numSamples - start);

            // The low cut, faded in and out as it is switched to and from Off.
            if (! lowCutFade.isOff())
            {
                if (lowCutLogHz.isSmoothing())
                {
                    lowCutLogHz.skip (length);
                    updateLowCut();
                }

                if (lowCutFade.isFull())
                {
                    lowCut.process (io, numSides, start, length);
                }
                else
                {
                    // The outer crossfade uses scratch; this one keeps its dry copy on the stack.
                    float dry[2][kCoefficientInterval];
                    for (int side = 0; side < numSides; ++side)
                        std::copy (io[side] + start, io[side] + start + length, dry[side]);
                    lowCut.process (io, numSides, start, length);
                    for (int n = 0; n < length; ++n)
                    {
                        const auto amount = lowCutFade.amount.getNextValue();
                        for (int side = 0; side < numSides; ++side)
                            io[side][start + n] = dry[side][n] + amount * (io[side][start + n] - dry[side][n]);
                    }
                    if (lowCutFade.isOff())
                        lowCut.clear();
                }
            }

            // Gliding bands are redesigned, then the active ones run in one cascade.
            std::array<Biquad*, kBands> active {};
            int numActive = 0;
            for (int i = 0; i < kBands; ++i)
            {
                auto& b = bands[(size_t) i];
                if (b.logHz.isSmoothing() || b.gainDb.isSmoothing() || b.q.isSmoothing())
                {
                    b.logHz.skip (length);
                    b.gainDb.skip (length);
                    b.q.skip (length);
                    updateBand (i);
                }
                if (b.active)
                    active[(size_t) numActive++] = &b.filter;
            }
            MixFilters::runFilters (active.data(), numActive, io, numSides, start, length);
        }
    });
}

double SendEqDsp::responseDb (const Settings& s, double hz, double rate)
{
    if (! s.on)
        return 0.0;

    const auto omega = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, 0.49 * rate, hz) / rate;
    double magnitude = 1.0;
    if (s.lowCutHz > 0.0f)
        magnitude *= MixFilters::CutFilter::magnitude (true, s.lowCutHz, false, omega, rate);

    const float hzs[] = { s.lowHz, s.midHz, s.highHz };
    const float db[] = { s.lowDb, s.midDb, s.highDb };
    for (int i = 0; i < kBands; ++i)
        if (std::abs (db[i]) >= 1.0e-4f)
            magnitude *= designBand (i, hzs[i], db[i], i == 1 ? s.midQ : 0.70710678, rate).magnitude (omega);

    return 20.0 * std::log10 (juce::jmax (1.0e-6, magnitude));
}
