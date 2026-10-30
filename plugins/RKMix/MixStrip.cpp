#include "MixStrip.h"

#include "MixLayout.h"

namespace
{
    using FVO = juce::FloatVectorOperations;

    using MixFilters::kSectionFadeSeconds;
    using MixFilters::kCoefficientInterval;
    constexpr double kDelayGlideSeconds = 0.05;

    template <typename T>
    void setValue (rkfx::Smoothed& v, T target, bool snap)
    {
        if (snap)
            v.setCurrentAndTargetValue ((float) target);
        else
            v.setTargetValue ((float) target);
    }
}

//==============================================================================
void StripDsp::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    for (auto& g : input)
        g.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    for (auto* fade : { &delayFade, &hpfFade, &gateFade, &eqFade, &compFade })
        fade->amount.reset (sampleRate, kSectionFadeSeconds);

    delaySamples.reset (sampleRate, kDelayGlideSeconds);
    for (auto& line : delay)
        line.prepare ((int) std::ceil (kMaxDelayMs * 0.001 * sampleRate) + 2);

    slopeFade.reset (sampleRate, kSectionFadeSeconds);
    hpfLogHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    gate.prepare (sampleRate);
    comp.prepare (sampleRate);

    for (auto& b : bands)
    {
        b.logHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.gainDb.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.q.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    }

    reset();
}

void StripDsp::reset()
{
    for (auto& line : delay)
        line.reset();
    for (auto& h : highPass)
        for (auto& stage : h.stages)
            stage.clear();
    gate.reset();
    comp.reset();
    for (auto& b : bands)
        b.filter.clear();
    setSettings (settings, true);
}

void StripDsp::setSettings (const Settings& s, bool snap)
{
    const auto old = settings;
    settings = s;

    const auto trim = mix::trimGain (s.trimDb);
    for (int side = 0; side < 2; ++side)
        setValue (input[(size_t) side], s.polarity[(size_t) side] ? -trim : trim, snap);

    if (delayFade.set (s.delayOn, snap))
        for (auto& line : delay)
            line.reset();
    setValue (delaySamples, juce::jlimit (0.0f, kMaxDelayMs, s.delayMs) * 0.001 * sampleRate, snap);

    // HPF: a slope change crossfades to the other filter, set up with the new slope.
    const auto stages = s.hpf.steep ? 2 : 1;
    setValue (hpfLogHz, std::log (juce::jlimit (10.0f, 20000.0f, s.hpf.hz)), snap);
    if (hpfFade.set (s.hpf.on, snap) || snap)
    {
        activeHighPass = 0;
        slopeFade.setCurrentAndTargetValue (0.0f);
        for (auto& stage : highPass[0].stages)
            stage.clear();
        designHighPass (highPass[0], s.hpf.hz, stages);
    }
    else if (stages != highPass[(size_t) activeHighPass].numStages && ! slopeFade.isSmoothing())
    {
        startSlopeChange();
    }
    else if (! hpfLogHz.isSmoothing() && snap)
    {
        designHighPass (highPass[(size_t) activeHighPass], s.hpf.hz, highPass[(size_t) activeHighPass].numStages);
    }

    if (gateFade.set (s.gate.on, snap))
        gate.reset();
    gate.setSettings ({ s.gate.thresholdDb, s.gate.rangeDb, s.gate.attackMs, s.gate.holdMs, s.gate.releaseMs });

    if (eqFade.set (s.eq.on, snap))
        for (auto& b : bands)
            b.filter.clear();
    for (int i = 0; i < kEqBands; ++i)
    {
        const auto& band = s.eq.bands[(size_t) i];
        auto& state = bands[(size_t) i];
        setValue (state.logHz, std::log (juce::jlimit (10.0f, 20000.0f, band.hz)), snap);
        setValue (state.gainDb, band.gainDb, snap);
        setValue (state.q, juce::jlimit (0.1f, 20.0f, band.q), snap);
        if (snap || band.bell != old.eq.bands[(size_t) i].bell)
            updateBand (i);
    }

    if (compFade.set (s.comp.on, snap))
        comp.reset();
    comp.setSettings ({ s.comp.thresholdDb, s.comp.ratio, s.comp.kneeDb, s.comp.attackMs, s.comp.releaseMs, s.comp.makeupDb });
}

float StripDsp::compTransferDb (const Comp& c, float inputDb)
{
    return MixDynamics::Compressor::transferDb ({ c.thresholdDb, c.ratio, c.kneeDb, c.attackMs, c.releaseMs, c.makeupDb }, inputDb);
}

//==============================================================================
void StripDsp::designHighPass (HighPass& h, double hz, int stages) const noexcept
{
    h.design (true, hz, stages, sampleRate);
}

rkfx::BiquadCoefficients StripDsp::designBand (int band, bool bell, double hz, double gainDb, double q, double rate)
{
    using namespace rkfx::MatchedDesign;
    if (band == 0 && ! bell)
        return lowShelf (hz, gainDb, rate);
    if (band == kEqBands - 1 && ! bell)
        return highShelf (hz, gainDb, rate);
    return rkfx::MatchedDesign::bell (hz, gainDb, q, rate);
}

void StripDsp::updateBand (int i) noexcept
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

double StripDsp::responseDb (const Settings& s, double hz, double rate)
{
    const auto omega = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, 0.49 * rate, hz) / rate;
    double magnitude = 1.0;

    if (s.hpf.on)
        magnitude *= HighPass::magnitude (true, s.hpf.hz, s.hpf.steep, omega, rate);

    if (s.eq.on)
        for (int i = 0; i < kEqBands; ++i)
        {
            const auto& band = s.eq.bands[(size_t) i];
            magnitude *= designBand (i, band.bell, band.hz, band.gainDb, band.q, rate).magnitude (omega);
        }

    return 20.0 * std::log10 (juce::jmax (1.0e-6, magnitude));
}

//==============================================================================
using MixFilters::runSection;

void StripDsp::processInput (const float* const* source, float* const* io, int numSides, int numSamples,
                             juce::AudioBuffer<float>& scratch) noexcept
{
    // Trim and polarity (a flip glides through silence)
    for (int side = 0; side < numSides; ++side)
    {
        auto& gain = input[(size_t) side];
        if (gain.isSmoothing())
        {
            for (int i = 0; i < numSamples; ++i)
                io[side][i] = source[side][i] * gain.getNextValue();
        }
        else if (const auto g = gain.getTargetValue(); juce::exactlyEqual (g, 1.0f))
        {
            FVO::copy (io[side], source[side], numSamples);
        }
        else
        {
            FVO::copyWithMultiply (io[side], source[side], g, numSamples);
        }
    }

    // Delay: steady times read whole samples (the usual case), a glide interpolates.
    runSection (delayFade, io, numSides, numSamples, scratch, [&]
    {
        if (! delaySamples.isSmoothing())
        {
            const auto d = delaySamples.getTargetValue();
            const auto whole = (int) d;
            const auto t = d - (float) whole;
            for (int side = 0; side < numSides; ++side)
            {
                auto& line = delay[(size_t) side];
                auto* x = io[side];
                if (t <= 0.0f)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        line.write (x[i]);
                        x[i] = line.readWhole (whole);
                    }
                }
                else
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        line.write (x[i]);
                        const auto a = line.readWhole (whole);
                        x[i] = a + t * (line.readWhole (whole + 1) - a);
                    }
                }
            }
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const auto d = delaySamples.getNextValue();
            const auto whole = (int) d;
            const auto t = d - (float) whole;
            for (int side = 0; side < numSides; ++side)
            {
                auto& line = delay[(size_t) side];
                line.write (io[side][i]);
                const auto a = line.readWhole (whole);
                io[side][i] = a + t * (line.readWhole (whole + 1) - a);
            }
        }
    });
}

void StripDsp::startSlopeChange() noexcept
{
    const auto stages = settings.hpf.steep ? 2 : 1;
    const auto hz = std::exp ((double) hpfLogHz.getCurrentValue());

    // While the HPF itself fades in or out, change at once (its crossfade covers it).
    if (! hpfFade.isFull())
    {
        designHighPass (highPass[(size_t) activeHighPass], hz, stages);
        return;
    }

    auto& next = highPass[(size_t) (1 - activeHighPass)];
    for (auto& stage : next.stages)
        stage.clear();
    designHighPass (next, hz, stages);
    slopeFade.setCurrentAndTargetValue (0.0f);
    slopeFade.setTargetValue (1.0f);
}

void StripDsp::processHpfAndGate (float* const* io, int numSides, int numSamples,
                                  juce::AudioBuffer<float>& scratch, const float* key) noexcept
{
    runSection (hpfFade, io, numSides, numSamples, scratch, [&]
    {
        for (int start = 0; start < numSamples; start += kCoefficientInterval)
        {
            const auto length = juce::jmin (kCoefficientInterval, numSamples - start);
            auto& active = highPass[(size_t) activeHighPass];

            if (hpfLogHz.isSmoothing())
            {
                hpfLogHz.skip (length);
                const auto hz = std::exp ((double) hpfLogHz.getCurrentValue());
                designHighPass (active, hz, active.numStages);
                if (slopeFade.isSmoothing())
                    designHighPass (highPass[(size_t) (1 - activeHighPass)], hz, highPass[(size_t) (1 - activeHighPass)].numStages);
            }

            if (! slopeFade.isSmoothing())
            {
                active.process (io, numSides, start, length);
                continue;
            }

            // A slope change: both filters run, crossfaded, then the new one takes over.
            float* other[2] = { scratch.getWritePointer (0), scratch.getWritePointer (1) };
            for (int side = 0; side < numSides; ++side)
                FVO::copy (other[side] + start, io[side] + start, length);
            active.process (io, numSides, start, length);
            highPass[(size_t) (1 - activeHighPass)].process (other, numSides, start, length);
            for (int n = start; n < start + length; ++n)
            {
                const auto t = slopeFade.getNextValue();
                for (int side = 0; side < numSides; ++side)
                    io[side][n] += t * (other[side][n] - io[side][n]);
            }
            if (! slopeFade.isSmoothing())
            {
                activeHighPass = 1 - activeHighPass;
                slopeFade.setCurrentAndTargetValue (0.0f);
                if (highPass[(size_t) activeHighPass].numStages != (settings.hpf.steep ? 2 : 1))
                    startSlopeChange();   // changed again during the fade
            }
        }
    });

    runSection (gateFade, io, numSides, numSamples, scratch, [&] { gate.process (io, numSides, numSamples, key); });
}

void StripDsp::processEq (float* const* io, int numSides, int numSamples, juce::AudioBuffer<float>& scratch) noexcept
{
    runSection (eqFade, io, numSides, numSamples, scratch, [&]
    {
        for (int start = 0; start < numSamples; start += kCoefficientInterval)
        {
            const auto length = juce::jmin (kCoefficientInterval, numSamples - start);

            // Gliding bands are redesigned, then every active band runs in one
            // loop (the bands' recursions overlap: much faster than one by one).
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
            MixFilters::runFilters (active.data(), numActive, io, numSides, start, length);
        }
    });
}

void StripDsp::processComp (float* const* io, int numSides, int numSamples,
                            juce::AudioBuffer<float>& scratch, const float* key) noexcept
{
    runSection (compFade, io, numSides, numSamples, scratch, [&] { comp.process (io, numSides, numSamples, key); });
}
