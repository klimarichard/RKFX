#include "PreampEngine.h"

#include <complex>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kMakeupLowHz = 100.0, kMakeupHighHz = 4000.0;   // the tone stack's loss is made up over this band, knobs centred
    constexpr float kReferenceLevel = 0.25f;     // -12 dBFS: level-matched through every stage
    constexpr float kBrightHz = 2500.0f, kMaxBrightDb = 6.0f;
    constexpr float kPresenceHz = 3500.0f, kPresenceRangeDb = 12.0f;

    /** Audio-taper pot (the bass pot): 0..1 -> 0..1, about 10% at the middle. */
    double audioTaper (double k) { return (std::exp (3.4 * k) - 1.0) / (std::exp (3.4) - 1.0); }

    float onePole (double hz, double rate) { return (float) std::exp (-2.0 * kPi * hz / rate); }
}

const PreampEngine::Voice& PreampEngine::voiceFor (Voicing v) noexcept
{
    // Tone stacks: classic published component values (Fender '59 Bassman,
    // Marshall JTM45 / 1959); the Top Boost uses a bright, high-mid stack in
    // the same topology, and the Hot-Rod a tighter Marshall-style one.
    static const Voice clean    { 1, 40.0f,  0.0f, 30.0f, 0.10f, 10000.0f, { 250e3, 1e6, 25e3, 56e3, 250e-12, 20e-9, 20e-9 } };
    static const Voice crunch   { 2, 70.0f,  6.0f, 42.0f, 0.20f, 7000.0f,  { 220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9 } };
    static const Voice topBoost { 2, 60.0f,  3.0f, 36.0f, 0.15f, 9000.0f,  { 1e6,   1e6, 10e3, 100e3, 50e-12, 22e-9, 22e-9 } };
    static const Voice hotRod   { 3, 120.0f, 12.0f, 60.0f, 0.25f, 6000.0f, { 250e3, 1e6, 25e3, 47e3, 470e-12, 22e-9, 22e-9 } };

    switch (v)
    {
        case Voicing::cleanAmerican: return clean;
        case Voicing::britishCrunch: return crunch;
        case Voicing::topBoost:      return topBoost;
        case Voicing::hotRod:        return hotRod;
    }
    return crunch;
}

PreampEngine::Analog PreampEngine::toneStack (const ToneStackParts& p, double t, double m, double bassKnob)
{
    // D. T. Yeh, J. O. Smith, "Discretization of the '59 Fender Bassman
    // tone stack", DAFx 2006.
    const auto l = audioTaper (bassKnob);
    const auto [R1, R2, R3, R4, C1, C2, C3] = p;

    Analog a;
    a.b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
    a.b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)
         - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
         + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
         + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
         + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
         + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
    a.b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
         - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
         + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
         + t * C1 * C2 * C3 * R1 * R3 * R4
         - t * m * C1 * C2 * C3 * R1 * R3 * R4
         + t * l * C1 * C2 * C3 * R1 * R2 * R4;
    a.a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
    a.a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
         + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
         - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
         + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
         + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
    a.a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
         - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
         + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
         + l * C1 * C2 * C3 * R1 * R2 * R4
         + C1 * C2 * C3 * R1 * R3 * R4;
    return a;
}

PreampEngine::Digital PreampEngine::discretise (const Analog& h, double rate)
{
    // Bilinear transform, s = c (1 - z^-1) / (1 + z^-1): s^k becomes
    // c^k (1 - z^-1)^k (1 + z^-1)^(3 - k).
    static constexpr double expand[4][4] = {
        { 1.0,  3.0,  3.0,  1.0 },     // (1 + z)^3
        { 1.0,  1.0, -1.0, -1.0 },     // (1 - z)(1 + z)^2
        { 1.0, -1.0, -1.0,  1.0 },     // (1 - z)^2 (1 + z)
        { 1.0, -3.0,  3.0, -1.0 }      // (1 - z)^3
    };

    const auto c = 2.0 * rate;
    const double num[4] = { 0.0, h.b1 * c, h.b2 * c * c, h.b3 * c * c * c };
    const double den[4] = { 1.0, h.a1 * c, h.a2 * c * c, h.a3 * c * c * c };

    Digital d {};
    for (int j = 0; j < 4; ++j)
        for (int k = 0; k < 4; ++k)
        {
            d.b[j] += num[k] * expand[k][j];
            d.a[j] += den[k] * expand[k][j];
        }

    const auto a0 = d.a[0];
    for (int j = 0; j < 4; ++j)
    {
        d.b[j] /= a0;
        d.a[j] /= a0;
    }
    return d;
}

namespace
{
    double magnitude (const PreampEngine::Digital& d, double hz, double rate)
    {
        const auto z1 = std::polar (1.0, -2.0 * kPi * hz / rate);
        std::complex<double> num = 0.0, den = 0.0, zk = 1.0;
        for (int k = 0; k < 4; ++k, zk *= z1)
        {
            num += d.b[k] * zk;
            den += d.a[k] * zk;
        }
        return std::abs (num / den);
    }

    /** Makes the centred stack's average level (in dB, over 100 Hz to 4 kHz)
        0 dB, so voicings and knob settings start equally loud. */
    double makeupFor (const PreampEngine::ToneStackParts& parts, double rate)
    {
        const auto centred = PreampEngine::discretise (PreampEngine::toneStack (parts, 0.5, 0.5, 0.5), rate);
        constexpr int points = 24;
        double sumDb = 0.0;
        for (int i = 0; i < points; ++i)
        {
            const auto hz = kMakeupLowHz * std::pow (kMakeupHighHz / kMakeupLowHz, i / (points - 1.0));
            sumDb += 20.0 * std::log10 (std::max (1.0e-6, magnitude (centred, hz, rate)));
        }
        return std::pow (10.0, -sumDb / points / 20.0);
    }
}

double PreampEngine::toneStackDb (const Settings& s, double rate, double hz)
{
    const auto& parts = voiceFor (s.voicing).stack;
    const auto d = discretise (toneStack (parts, s.treble, s.middle, s.bass), rate);
    return 20.0 * std::log10 (magnitude (d, hz, rate) * makeupFor (parts, rate));
}

float PreampEngine::stageMakeup (float gain, float bias) noexcept
{
    const auto out = 0.5f * (std::abs (stageCurve (gain * kReferenceLevel, bias)) + std::abs (stageCurve (-gain * kReferenceLevel, bias)));
    return out > 1.0e-9f ? kReferenceLevel / out : 1.0f;
}

float PreampEngine::stageGainFor (float gainKnob) const noexcept
{
    const auto& v = voiceFor (voicing);
    const auto totalDb = v.minGainDb + (v.maxGainDb - v.minGainDb) * juce::jlimit (0.0f, 1.0f, gainKnob);
    return juce::Decibels::decibelsToGain (totalDb / (float) v.stages);
}

//==============================================================================
void PreampEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fadeLength = juce::jmax (1, (int) (0.01 * sampleRate));

    for (auto* s : { &gain, &bass, &middle, &treble, &presence, &brightDb })
        s->reset (sampleRate, rkfx::kParamSmoothingSeconds);
    switchFade.reset (sampleRate, rkfx::kBypassFadeSeconds);

    setSettings (settings);
    reset();
}

void PreampEngine::reset()
{
    for (auto* s : { &gain, &bass, &middle, &treble, &presence, &brightDb })
        s->setCurrentAndTargetValue (s->getTargetValue());
    switchFade.setCurrentAndTargetValue (1.0f);

    for (auto& c : channels)
        c.bright.reset();
    presenceFilter.reset();
    shownBright = shownPresence = -100.0f;

    cabType = settings.cab;
    activeCab = 0;
    cabFadeRemaining = 0;
    cabs[0].configure (cabType, sampleRate);
    for (auto& cab : cabs)
        cab.reset();

    applyVoicing();
}

void PreampEngine::setSettings (const Settings& s)
{
    settings = s;
    gain.setTargetValue (s.gain);
    bass.setTargetValue (s.bass);
    middle.setTargetValue (s.middle);
    treble.setTargetValue (s.treble);
    presence.setTargetValue (s.presence);
    brightDb.setTargetValue (s.bright ? kMaxBrightDb * (1.0f - 0.6f * s.gain) : 0.0f);
}

void PreampEngine::applyVoicing() noexcept
{
    voicing = settings.voicing;
    factor = settings.oversampling;
    const auto& v = voiceFor (voicing);
    const auto osRate = sampleRate * factor;

    oversampler.reset();
    interstageCoef = onePole (juce::jmin ((double) v.interstageHz, 0.4 * osRate), osRate);
    for (auto& c : channels)
    {
        c.inputHighPass.prepare (sampleRate);
        c.inputHighPass.setCutoff (v.inputHighPassHz);
        for (auto& dc : c.dc)
            dc.prepare (osRate);
        c.lowPass.fill (0.0f);
        c.stack.reset();
    }

    stackMakeup = makeupFor (v.stack, sampleRate);
    stageGain = stageGainFor (gain.getCurrentValue());
    makeup = stageMakeup (stageGain, v.bias);
    updateStack();
}

void PreampEngine::updateStack() noexcept
{
    const auto t = treble.getNextValue(), m = middle.getNextValue(), b = bass.getNextValue();
    for (auto* s : { &treble, &middle, &bass })
        s->skip (kSubBlock - 1);
    stackCoefs = discretise (toneStack (voiceFor (voicing).stack, t, m, b), sampleRate);
    untilUpdate = kSubBlock;
}

void PreampEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    if (numChannels <= 0 || numSamples <= 0)
        return;

    // Voicing or oversampling changed: dip, switch while silent, come back.
    if (settings.voicing != voicing || settings.oversampling != factor)
    {
        if (! switchFade.isSmoothing() && switchFade.getCurrentValue() <= 0.0f)
        {
            applyVoicing();
            switchFade.setTargetValue (1.0f);
        }
        else
        {
            switchFade.setTargetValue (0.0f);
        }
    }

    // A cab change crossfades the old and new cabinets.
    if (settings.cab != cabType && cabFadeRemaining == 0)
    {
        cabType = settings.cab;
        auto& next = cabs[(size_t) (1 - activeCab)];
        next.configure (cabType, sampleRate);
        next.reset();
        cabFadeRemaining = fadeLength * 2;
    }

    // Bright and presence follow once per block.
    if (const auto db = brightDb.skip (numSamples); std::abs (db - shownBright) > 1.0e-3f)
    {
        shownBright = db;
        const auto coefs = rkfx::MatchedDesign::highShelf (kBrightHz, db, sampleRate);
        for (auto& c : channels)
            c.bright.setCoefficients (coefs);
    }
    if (const auto p = presence.skip (numSamples); std::abs (p - shownPresence) > 1.0e-4f)
    {
        shownPresence = p;
        presenceFilter.setCoefficients (rkfx::MatchedDesign::highShelf (kPresenceHz, (p - 0.5f) * kPresenceRangeDb, sampleRate));
    }

    // Input: coupling high-pass and bright, at the base rate.
    const auto brightOn = std::abs (shownBright) > 1.0e-3f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* data = io[ch];
        for (int i = 0; i < numSamples; ++i)
        {
            auto x = c.inputHighPass.process (data[i], rkfx::StateVariableFilter::Type::highpass);
            data[i] = brightOn ? c.bright.process (ch, x) : x;
        }
    }

    // Gain stages, oversampled; the drive glides across the block.
    const auto& v = voiceFor (voicing);
    const auto gainStart = stageGain, makeupStart = makeup;
    stageGain = stageGainFor (gain.skip (numSamples));
    makeup = stageMakeup (stageGain, v.bias);
    const auto gainStep = (stageGain - gainStart) / (float) numSamples;
    const auto makeupStep = (makeup - makeupStart) / (float) numSamples;
    const auto stages = v.stages;
    const auto bias = v.bias;

    oversampler.process (io, numChannels, numSamples, factor, [&] (int ch, int index, float x)
    {
        auto& c = channels[(size_t) ch];
        const auto g = gainStart + gainStep * (float) (index + 1);
        const auto mk = makeupStart + makeupStep * (float) (index + 1);
        for (int s = 0; s < stages; ++s)
        {
            x = stageCurve (g * x, bias) * mk;
            x = c.dc[(size_t) s].process (x);
            c.lowPass[(size_t) s] = x + interstageCoef * (c.lowPass[(size_t) s] - x);
            x = c.lowPass[(size_t) s];
        }
        return x;
    });

    // Tone stack, presence, cab, switching dip.
    for (int i = 0; i < numSamples; ++i)
    {
        if (--untilUpdate <= 0)
            updateStack();

        const auto fade = switchFade.getNextValue();
        const auto cabFade = cabFadeRemaining > 0 ? 1.0f - (float) cabFadeRemaining / (float) (fadeLength * 2) : 1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            auto x = (float) (c.stack.process (stackCoefs, io[ch][i]) * stackMakeup);
            x = presenceFilter.process (ch, x);

            auto y = cabs[(size_t) activeCab].process (ch, x);
            if (cabFadeRemaining > 0)
                y += cabFade * (cabs[(size_t) (1 - activeCab)].process (ch, x) - y);

            io[ch][i] = fade * y;
        }

        if (cabFadeRemaining > 0 && --cabFadeRemaining == 0)
            activeCab = 1 - activeCab;
    }
}
