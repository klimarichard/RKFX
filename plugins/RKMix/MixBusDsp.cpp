#include "MixBusDsp.h"

namespace
{
    using MixFilters::kCoefficientInterval;
    using MixFilters::runSection;

    void setValue (rkfx::Smoothed& v, double target, bool snap)
    {
        if (snap)
            v.setCurrentAndTargetValue ((float) target);
        else
            v.setTargetValue ((float) target);
    }
}

void BusDsp::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    scratch.setSize (2, juce::jmax (1, maxBlockSize));

    for (auto* fade : { &hpf.fade, &lpf.fade, &eqFade, &compFade, &limitFade })
        fade->amount.reset (sampleRate, MixFilters::kSectionFadeSeconds);
    hpf.highPass = true;
    lpf.highPass = false;
    for (auto* cut : { &hpf, &lpf })
        cut->logHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    for (auto& b : bands)
    {
        b.logHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.gainDb.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.q.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    }
    comp.prepare (sampleRate);
    limiter.prepare (sampleRate);
    reset();
}

void BusDsp::reset()
{
    for (auto* cut : { &hpf, &lpf })
    {
        cut->filter.clear();
        cut->changingSlope = false;
    }
    for (auto& b : bands)
        b.filter.clear();
    comp.reset();
    limiter.reset();
    setSettings (settings, true);
}

void BusDsp::setCut (Cut& cut, bool on, float hz, bool steep, bool snap)
{
    const auto stages = steep ? 2 : 1;
    setValue (cut.logHz, std::log (juce::jlimit (10.0f, 22000.0f, hz)), snap);
    cut.wanted = on;
    cut.steep = steep;

    // A slope change under way finishes first (it then follows wanted and steep).
    if (cut.changingSlope && ! snap)
        return;

    if (cut.fade.set (on, snap) || snap)
    {
        cut.filter.clear();
        cut.filter.design (cut.highPass, std::exp ((double) cut.logHz.getCurrentValue()), stages, sampleRate);
        cut.changingSlope = false;
    }
    else if (stages != cut.filter.numStages && cut.fade.isFull())
    {
        // A new slope: the filter dips out, changes, and comes back (no click).
        cut.changingSlope = true;
        cut.fade.amount.setTargetValue (0.0f);
    }
    else if (stages != cut.filter.numStages)
    {
        cut.filter.design (cut.highPass, std::exp ((double) cut.logHz.getCurrentValue()), stages, sampleRate);
    }
}

void BusDsp::setSettings (const Settings& s, bool snap)
{
    const auto old = settings;
    settings = s;

    setCut (hpf, s.eq.on && s.eq.hpfOn, s.eq.hpfHz, s.eq.hpfSteep, snap);
    setCut (lpf, s.eq.on && s.eq.lpfOn, s.eq.lpfHz, s.eq.lpfSteep, snap);

    if (eqFade.set (s.eq.on, snap))
        for (auto& b : bands)
            b.filter.clear();
    for (int i = 0; i < kEqBands; ++i)
    {
        const auto& band = s.eq.bands[(size_t) i];
        auto& state = bands[(size_t) i];
        setValue (state.logHz, std::log (juce::jlimit (10.0f, 22000.0f, band.hz)), snap);
        setValue (state.gainDb, band.gainDb, snap);
        setValue (state.q, juce::jlimit (0.1f, 20.0f, band.q), snap);
        if (snap || band.bell != old.eq.bands[(size_t) i].bell)
            updateBand (i);
    }

    if (compFade.set (s.comp.on, snap))
        comp.reset();
    comp.setSettings ({ s.comp.thresholdDb, s.comp.ratio, s.comp.kneeDb, s.comp.attackMs, s.comp.releaseMs,
                        s.comp.makeupDb, s.comp.autoRelease });

    if (limitFade.set (s.limit.on, snap))
        limiter.reset();
    limiter.setSettings ({ s.limit.ceilingDb, s.limit.releaseMs });
}

bool BusDsp::isAnyOn() const noexcept
{
    for (const auto* fade : { &hpf.fade, &lpf.fade, &eqFade, &compFade, &limitFade })
        if (! fade->isOff())
            return true;
    return false;
}

//==============================================================================
rkfx::BiquadCoefficients BusDsp::designBand (int band, bool bell, double hz, double gainDb, double q, double rate)
{
    using namespace rkfx::MatchedDesign;
    if (band == 0 && ! bell)
        return lowShelf (hz, gainDb, rate);
    if (band == kEqBands - 1 && ! bell)
        return highShelf (hz, gainDb, rate);
    return rkfx::MatchedDesign::bell (hz, gainDb, q, rate);
}

void BusDsp::updateBand (int i) noexcept
{
    auto& b = bands[(size_t) i];
    const auto gainDb = b.gainDb.getCurrentValue();
    const auto wasActive = b.active;
    b.active = std::abs (gainDb) >= 1.0e-4f || b.gainDb.isSmoothing();
    if (! b.active)
        return;
    if (! wasActive)
        b.filter.clear();
    b.filter.set (designBand (i, settings.eq.bands[(size_t) i].bell, std::exp ((double) b.logHz.getCurrentValue()),
                              gainDb, b.q.getCurrentValue(), sampleRate));
}

double BusDsp::responseDb (const Eq& eq, double hz, double rate)
{
    if (! eq.on)
        return 0.0;

    const auto omega = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, 0.49 * rate, hz) / rate;
    double magnitude = 1.0;
    if (eq.hpfOn)
        magnitude *= CutFilter::magnitude (true, eq.hpfHz, eq.hpfSteep, omega, rate);
    if (eq.lpfOn)
        magnitude *= CutFilter::magnitude (false, eq.lpfHz, eq.lpfSteep, omega, rate);
    for (int i = 0; i < kEqBands; ++i)
    {
        const auto& band = eq.bands[(size_t) i];
        magnitude *= designBand (i, band.bell, band.hz, band.gainDb, band.q, rate).magnitude (omega);
    }
    return 20.0 * std::log10 (juce::jmax (1.0e-6, magnitude));
}

float BusDsp::compTransferDb (const Comp& c, float inputDb)
{
    return MixDynamics::Compressor::transferDb ({ c.thresholdDb, c.ratio, c.kneeDb, c.attackMs, c.releaseMs, c.makeupDb }, inputDb);
}

//==============================================================================
void BusDsp::processCut (Cut& cut, float* const* io, int numSides, int numSamples) noexcept
{
    runSection (cut.fade, io, numSides, numSamples, scratch, [&]
    {
        for (int start = 0; start < numSamples; start += kCoefficientInterval)
        {
            const auto length = juce::jmin (kCoefficientInterval, numSamples - start);
            if (cut.logHz.isSmoothing())
            {
                cut.logHz.skip (length);
                cut.filter.design (cut.highPass, std::exp ((double) cut.logHz.getCurrentValue()), cut.filter.numStages, sampleRate);
            }
            cut.filter.process (io, numSides, start, length);
        }
    });

    // Dipped out for a slope change: change it and come back in.
    if (cut.changingSlope && cut.fade.isOff())
    {
        cut.changingSlope = false;
        cut.filter.clear();
        cut.filter.design (cut.highPass, std::exp ((double) cut.logHz.getCurrentValue()), cut.steep ? 2 : 1, sampleRate);
        cut.fade.amount.setTargetValue (cut.wanted ? 1.0f : 0.0f);
    }
}

void BusDsp::processBeforeFader (float* const* io, int numSides, int numSamples) noexcept
{
    processCut (hpf, io, numSides, numSamples);

    runSection (eqFade, io, numSides, numSamples, scratch, [&]
    {
        for (int start = 0; start < numSamples; start += kCoefficientInterval)
        {
            const auto length = juce::jmin (kCoefficientInterval, numSamples - start);
            std::array<Biquad*, kEqBands> active {};
            int numActive = 0;
            for (int i = 0; i < kEqBands; ++i)
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
            if (numActive > 0)
                MixFilters::runFilters (active.data(), numActive, io, numSides, start, length);
        }
    });

    processCut (lpf, io, numSides, numSamples);

    runSection (compFade, io, numSides, numSamples, scratch, [&] { comp.process (io, numSides, numSamples, nullptr); });
}

void BusDsp::processLimiter (float* const* io, int numSides, int numSamples) noexcept
{
    runSection (limitFade, io, numSides, numSamples, scratch, [&] { limiter.process (io, numSides, numSamples); });
}
