#include "DistortEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kMidHz = 800.0;
    constexpr double kMidQ = 0.7;
    constexpr float kFuzzBias = 0.35f;
    constexpr float kFuzzDeadZone = 0.003f;   // about -50 dBFS at the input

    float onePole (double hz, double rate) { return (float) std::exp (-2.0 * kPi * hz / rate); }

    struct CabCurve
    {
        double highPassHz; int highPassStages;
        double lowPassHz;  int lowPassStages;
        struct Bell { double hz, gainDb, q; } bells[3];
    };

    CabCurve curveFor (DistortEngine::Cab cab)
    {
        using Cab = DistortEngine::Cab;
        switch (cab)
        {
            case Cab::small:  return { 100.0, 1, 5000.0, 2, { { 120.0,  4.0, 1.5 }, { 2500.0, 3.0, 1.0 }, { 800.0, -1.5, 0.8 } } };
            case Cab::medium: return { 80.0,  1, 5500.0, 2, { { 110.0,  3.0, 1.2 }, { 2000.0, 2.5, 1.0 }, { 500.0, -1.0, 0.8 } } };
            case Cab::large:  return { 70.0,  1, 4500.0, 2, { { 100.0,  4.0, 1.2 }, { 2200.0, 3.0, 1.2 }, { 400.0, -3.0, 1.0 } } };
            case Cab::bass:   return { 45.0,  1, 3500.0, 2, { { 80.0,   3.0, 1.0 }, { 1200.0, 1.5, 1.0 }, { 600.0, -2.0, 1.0 } } };
            case Cab::off:    break;
        }
        return {};
    }
}

//==============================================================================
void DistortEngine::CabFilter::configure (Cab cab, double rate)
{
    off = cab == Cab::off;
    if (off)
        return;

    const auto curve = curveFor (cab);
    highPass.configure (rkfx::FilterCascade::Mode::highpass, curve.highPassStages, false);
    lowPass.configure (rkfx::FilterCascade::Mode::lowpass, curve.lowPassStages, false);
    highPassG = rkfx::StateVariableFilter::gFor (curve.highPassHz, rate);
    lowPassG = rkfx::StateVariableFilter::gFor (curve.lowPassHz, rate);

    for (size_t i = 0; i < 3; ++i)
    {
        const auto& b = curve.bells[i];
        bellCoefs[i] = rkfx::MatchedDesign::bell (b.hz, b.gainDb, b.q, rate);
        bells[i].setCoefficients (bellCoefs[i]);
    }
}

void DistortEngine::CabFilter::reset()
{
    highPass.reset();
    lowPass.reset();
    for (auto& b : bells)
        b.reset();
}

float DistortEngine::CabFilter::process (int channel, float x) noexcept
{
    if (off)
        return x;

    x = highPass.process (channel, x, highPassG, 0.0f);
    for (auto& b : bells)
        x = b.process (channel, x);
    return lowPass.process (channel, x, lowPassG, 0.0f);
}

double DistortEngine::CabFilter::magnitudeDb (double hz, double rate) const
{
    if (off)
        return 0.0;

    const auto w = 2.0 * kPi * hz / rate;
    auto m = highPass.magnitude (w, highPassG, 0.0) * lowPass.magnitude (w, lowPassG, 0.0);
    for (const auto& c : bellCoefs)
        m *= c.magnitude (w);
    return 20.0 * std::log10 (std::max (m, 1.0e-10));
}

//==============================================================================
float DistortEngine::clip (Type t, float x) noexcept
{
    switch (t)
    {
        case Type::overdrive:
            return rkfx::softClip (x);

        case Type::distortion:
        {
            // Firmer knee than the overdrive: cubic, flat from |x| = 1.5.
            const auto u = juce::jlimit (-1.0f, 1.0f, x / 1.5f);
            return 1.5f * u - 0.5f * u * u * u;
        }

        case Type::fuzz:
        {
            // Biased: the positive side clips hard and early, the negative side softly.
            const auto biased = x + kFuzzBias;
            const auto shaped = biased > 0.0f ? juce::jmin (biased, 0.8f) : rkfx::softClip (biased);
            return shaped - kFuzzBias;
        }
    }
    return x;
}

void DistortEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    coef600 = onePole (600.0, sampleRate);
    coef200 = onePole (200.0, sampleRate);
    coefPost = onePole (juce::jmin (6000.0, 0.4 * sampleRate), sampleRate);
    fadeLength = juce::jmax (1, (int) (0.01 * sampleRate));

    for (auto& c : channels)
    {
        c.tight.prepare (sampleRate);
        c.dc.prepare (sampleRate);
        c.tilt.prepare (sampleRate);
    }

    gain.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    outputGain.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    for (auto* s : { &cut600, &cut200, &postSmooth, &deadZone })
        s->reset (sampleRate, rkfx::kBypassFadeSeconds);
    midDb.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void DistortEngine::reset()
{
    for (auto& c : channels)
    {
        c.tight.reset();
        c.pre600 = c.pre200 = c.post = 0.0f;
        c.dc.reset();
        c.mid.reset();
        c.tilt.reset();
    }
    tightRunning = settings.tightHz > 0.0f;

    oversampler.reset();
    factor = settings.oversampling;
    previousType = type = settings.type;
    typeFadeRemaining = 0;

    cabType = settings.cab;
    activeCab = 0;
    cabFadeRemaining = 0;
    cabs[0].configure (cabType, sampleRate);
    for (auto& cab : cabs)
        cab.reset();

    gain.setCurrentAndTargetValue (gain.getTargetValue());
    outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
    for (auto* s : { &cut600, &cut200, &postSmooth, &deadZone, &midDb })
        s->setCurrentAndTargetValue (s->getTargetValue());

    midDbShown = 1.0e9f;   // force the mid coefficients on the next block
}

void DistortEngine::setSettings (const Settings& s)
{
    settings = s;
    settings.oversampling = s.oversampling >= 4 ? 4 : s.oversampling >= 2 ? 2 : 1;

    const auto gainDb = juce::jlimit (0.0f, 60.0f, s.gainDb);
    gain.setTargetValue (juce::Decibels::decibelsToGain (gainDb));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (s.autoGain ? autoGainDb (gainDb) : 0.0f));
    midDb.setTargetValue (juce::jlimit (-12.0f, 12.0f, s.midDb));

    // Pre-clip low cut (mid focus), post-clip smoothing and fuzz dead zone, per type.
    cut600.setTargetValue (s.type == Type::overdrive ? 0.75f : 0.0f);
    cut200.setTargetValue (s.type == Type::distortion ? 0.4f : 0.0f);
    postSmooth.setTargetValue (s.type == Type::overdrive ? 1.0f : s.type == Type::distortion ? 0.5f : 0.0f);
    deadZone.setTargetValue (s.type == Type::fuzz ? kFuzzDeadZone : 0.0f);

    for (auto& c : channels)
        c.tilt.setTone (juce::jlimit (-1.0f, 1.0f, s.tone));
}

void DistortEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    if (settings.oversampling != factor)
    {
        factor = settings.oversampling;
        oversampler.reset();
    }

    if (settings.type != type && typeFadeRemaining == 0)
    {
        previousType = type;
        type = settings.type;
        typeFadeRemaining = fadeLength;
    }

    if (settings.cab != cabType && cabFadeRemaining == 0)
    {
        cabType = settings.cab;
        auto& next = cabs[(size_t) (1 - activeCab)];
        next.configure (cabType, sampleRate);
        next.reset();
        cabFadeRemaining = fadeLength * 2;
    }

    // Tight: a 12 dB/oct high-pass before the gain.
    const auto tightOn = settings.tightHz > 0.0f;
    for (auto& c : channels)
    {
        if (tightOn && ! tightRunning)
            c.tight.reset();
        if (tightOn)
            c.tight.setCutoff (settings.tightHz);
    }
    tightRunning = tightOn;

    // Mid: coefficients follow the smoothed value once per block.
    const auto mid = midDb.skip (numSamples);
    if (std::abs (mid - midDbShown) > 1.0e-4f)
    {
        midDbShown = mid;
        const auto coefs = rkfx::MatchedDesign::bell (kMidHz, mid, kMidQ, sampleRate);
        for (auto& c : channels)
            c.mid.setCoefficients (coefs);
    }

    // Pre-shaping and gain at the base rate.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = gain.getNextValue();
        const auto a600 = cut600.getNextValue();
        const auto a200 = cut200.getNextValue();
        const auto dz = deadZone.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            auto x = io[ch][i];

            if (tightOn)
                x = c.tight.process (x, rkfx::StateVariableFilter::Type::highpass);

            c.pre600 = x + coef600 * (c.pre600 - x);
            c.pre200 = x + coef200 * (c.pre200 - x);
            x -= a600 * c.pre600 + a200 * c.pre200;

            // Fuzz dead zone: very quiet signals fall through, so notes sputter as they decay.
            if (dz > 0.0f)
                x = x > dz ? x - dz : x < -dz ? x + dz : 0.0f;

            io[ch][i] = x * g;
        }
    }

    // The clipper, oversampled; type changes crossfade old and new curves.
    const auto fadeStart = typeFadeRemaining;
    const auto oldType = previousType, newType = type;
    oversampler.process (io, numChannels, numSamples, factor, [&] (int, int index, float x)
    {
        const auto newY = clip (newType, x);
        const auto remaining = fadeStart - index;
        if (remaining <= 0)
            return newY;
        const auto oldY = clip (oldType, x);
        return oldY + (1.0f - (float) remaining / (float) fadeLength) * (newY - oldY);
    });
    typeFadeRemaining = juce::jmax (0, typeFadeRemaining - numSamples);

    // Post-shaping, mid, tone, cab, auto gain.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto smooth = postSmooth.getNextValue();
        const auto out = outputGain.getNextValue();
        const auto cabFade = cabFadeRemaining > 0 ? 1.0f - (float) cabFadeRemaining / (float) (fadeLength * 2) : 1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            auto x = c.dc.process (io[ch][i]);

            c.post = x + coefPost * (c.post - x);
            x += smooth * (c.post - x);

            x = c.mid.process (ch, x);   // exact identity at 0 dB
            x = c.tilt.process (x);

            auto y = cabs[(size_t) activeCab].process (ch, x);
            if (cabFadeRemaining > 0)
                y += cabFade * (cabs[(size_t) (1 - activeCab)].process (ch, x) - y);

            io[ch][i] = out * y;
        }

        if (cabFadeRemaining > 0 && --cabFadeRemaining == 0)
            activeCab = 1 - activeCab;
    }
}
