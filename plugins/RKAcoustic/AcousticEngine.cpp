#include "AcousticEngine.h"

#include <cmath>

namespace
{
    constexpr float kLowHz = 100.0f, kTrebleHz = 4000.0f, kAirHz = 12000.0f;
    constexpr float kMidQ = 0.9f, kQuackQ = 1.4f, kShelfQ = 0.70710678f;
    constexpr float kHighPassOffHz = 10.0f;
    constexpr float kKneeDb = 6.0f, kAttackMs = 10.0f;

    float toOctave (float hz) noexcept { return std::log2 (juce::jmax (1.0f, hz)); }
}

const AcousticEngine::Voice& AcousticEngine::voiceFor (Voicing v) noexcept
{
    // Body: the main air and body resonances (boosted) and the boxy or
    // nasal region a pickup over-states (cut). Quack: the piezo's hard peak.
    static const Voice flat   { { { { 300.0f, 0.0f, 1.0f }, { 600.0f, 0.0f, 1.0f }, { 1200.0f, 0.0f, 1.0f } } }, 2500.0f };
    static const Voice violin { { { { 280.0f, 5.0f, 3.0f }, { 470.0f, 4.0f, 3.5f }, { 1500.0f, -2.0f, 1.2f } } }, 3000.0f };
    static const Voice viola  { { { { 230.0f, 5.0f, 3.0f }, { 380.0f, 4.0f, 3.5f }, { 1200.0f, -2.0f, 1.2f } } }, 2600.0f };
    static const Voice guitar { { { { 100.0f, 5.0f, 2.5f }, { 200.0f, 3.0f, 2.5f }, {  800.0f, -2.0f, 1.2f } } }, 1600.0f };
    static const Voice bass   { { { {  65.0f, 4.0f, 2.5f }, { 110.0f, 4.0f, 3.0f }, {  400.0f, -2.0f, 1.2f } } },  900.0f };

    switch (v)
    {
        case Voicing::flat:   return flat;
        case Voicing::violin: return violin;
        case Voicing::viola:  return viola;
        case Voicing::guitar: return guitar;
        case Voicing::bass:   return bass;
    }
    return flat;
}

float AcousticEngine::makeupFor (float compress) noexcept
{
    // A quarter of the reduction at the threshold's distance below 0 dBFS:
    // up to about 5.6 dB, so quiet passages don't come up too much.
    return 0.25f * -thresholdFor (compress) * (1.0f - 1.0f / ratioFor (compress));
}

std::array<AcousticEngine::Target, AcousticEngine::kNumBands> AcousticEngine::targetsFor (const Settings& s)
{
    const auto& voice = voiceFor (s.voicing);
    std::array<Target, kNumBands> t;
    for (int i = 0; i < 3; ++i)
    {
        const auto& r = voice.body[(size_t) i];
        t[(size_t) (body1 + i)] = { Shape::bell, r.hz, r.gainDb * s.body, r.q };
    }
    t[quackBand]  = { Shape::bell, voice.quackHz, -kMaxQuackCutDb * s.quack, kQuackQ };
    t[lowBand]    = { Shape::lowShelf, kLowHz, s.lowDb, kShelfQ };
    t[midBand]    = { Shape::bell, s.midHz, s.midDb, kMidQ };
    t[trebleBand] = { Shape::highShelf, kTrebleHz, s.trebleDb, kShelfQ };
    t[airBand]    = { Shape::highShelf, kAirHz, s.airDb, kShelfQ };
    // Off: the notch glides away (0 dB) where it was, and is skipped.
    t[notchBand]  = { Shape::bell, s.notchHz > 0.0f ? s.notchHz : 1000.0f, s.notchHz > 0.0f ? kNotchDepthDb : 0.0f, kNotchQ };
    return t;
}

rkfx::BiquadCoefficients AcousticEngine::design (Shape shape, double hz, double gainDb, double q, double rate)
{
    switch (shape)
    {
        case Shape::bell:      return rkfx::MatchedDesign::bell (hz, gainDb, q, rate);
        case Shape::lowShelf:  return rkfx::MatchedDesign::lowShelf (hz, gainDb, rate, q);
        case Shape::highShelf: return rkfx::MatchedDesign::highShelf (hz, gainDb, rate, q);
    }
    return {};
}

double AcousticEngine::magnitudeDb (const Settings& s, double rate, double hz)
{
    const auto omega = 2.0 * juce::MathConstants<double>::pi * hz / rate;
    double db = 0.0;
    for (const auto& t : targetsFor (s))
        if (std::abs (t.gainDb) > 1.0e-4f)
            db += 20.0 * std::log10 (design (t.shape, t.hz, t.gainDb, t.q, rate).magnitude (omega));

    if (s.highPassHz > 0.0f)
    {
        // 4th-order Butterworth high-pass.
        const auto ratio = s.highPassHz / hz;
        db -= 10.0 * std::log10 (1.0 + std::pow (ratio, 8.0));
    }
    return db;
}

void AcousticEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& f : highPass1) f.prepare (sampleRate);
    for (auto& f : highPass2) f.prepare (sampleRate);

    for (auto& b : bands)
        for (auto* s : { &b.octave, &b.gainDb, &b.q })
            s->reset (sampleRate / kSubBlock, rkfx::kParamSmoothingSeconds);
    highPassOctave.reset (sampleRate / kSubBlock, rkfx::kParamSmoothingSeconds);
    compressAmount.reset (sampleRate / kSubBlock, rkfx::kParamSmoothingSeconds);
    polarity.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    detector.prepare (sampleRate);
    detector.setMode (rkfx::Dynamics::LevelDetector::Mode::rms);
    gainSmoother.prepare (sampleRate);
    gainSmoother.setTimes (kAttackMs, 150.0f);
    gainSmoother.setAutoRelease (true);

    setSettings (settings);
    reset();
}

void AcousticEngine::reset()
{
    for (auto& f : highPass1) f.reset();
    for (auto& f : highPass2) f.reset();
    for (auto& b : bands)
        b.filter.reset();
    detector.reset();
    gainSmoother.reset();

    polarity.setCurrentAndTargetValue (settings.invert ? -1.0f : 1.0f);
    updateFilters (true);
}

void AcousticEngine::setSettings (const Settings& s)
{
    settings = s;

    const auto targets = targetsFor (s);
    for (int i = 0; i < kNumBands; ++i)
    {
        auto& b = bands[(size_t) i];
        b.shape = targets[(size_t) i].shape;
        b.octave.setTargetValue (toOctave (targets[(size_t) i].hz));
        b.gainDb.setTargetValue (targets[(size_t) i].gainDb);
        b.q.setTargetValue (targets[(size_t) i].q);
    }

    highPassOctave.setTargetValue (toOctave (s.highPassHz > 0.0f ? s.highPassHz : kHighPassOffHz));
    compressAmount.setTargetValue (juce::jlimit (0.0f, 1.0f, s.compress));
    polarity.setTargetValue (s.invert ? -1.0f : 1.0f);
}

void AcousticEngine::updateFilters (bool snap) noexcept
{
    for (auto& b : bands)
    {
        if (snap)
            for (auto* s : { &b.octave, &b.gainDb, &b.q })
                s->setCurrentAndTargetValue (s->getTargetValue());

        const auto moving = b.octave.isSmoothing() || b.gainDb.isSmoothing() || b.q.isSmoothing();
        const auto octave = b.octave.getNextValue(), gainDb = b.gainDb.getNextValue(), q = b.q.getNextValue();

        // Coefficients only while gliding (or on a snap); a flat band rests.
        const auto wasActive = b.active;
        b.active = std::abs (gainDb) > 1.0e-3f;
        if (b.active && (moving || snap || ! wasActive))
            b.filter.setCoefficients (design (b.shape, std::exp2 (octave), gainDb, q, sampleRate));
        if (! b.active)
            b.filter.reset();
    }

    if (snap)
    {
        highPassOctave.setCurrentAndTargetValue (highPassOctave.getTargetValue());
        compressAmount.setCurrentAndTargetValue (compressAmount.getTargetValue());
    }
    // Parked at 10 Hz when off: skipped, and starts clean when switched on
    // (a high-pass passes the signal straight away, so that never clicks).
    const auto wasActive = highPassActive;
    highPassActive = settings.highPassHz > 0.0f || highPassOctave.isSmoothing();
    if (highPassActive && ! wasActive)
    {
        for (auto& f : highPass1) f.reset();
        for (auto& f : highPass2) f.reset();
    }

    const auto hz = std::exp2 (highPassOctave.getNextValue());
    for (auto& f : highPass1) f.setCutoff (hz, 0.54119610f);   // two stages: 24 dB/oct Butterworth
    for (auto& f : highPass2) f.setCutoff (hz, 1.30656296f);
    compressAmount.getNextValue();

    untilUpdate = kSubBlock;
}

void AcousticEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    if (numChannels <= 0)
        return;

    using Type = rkfx::StateVariableFilter::Type;

    for (int i = 0; i < numSamples; ++i)
    {
        if (--untilUpdate <= 0)
            updateFilters (false);

        const auto sign = polarity.getNextValue();
        float level = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto x = io[ch][i];
            if (highPassActive)
                x = highPass2[(size_t) ch].process (highPass1[(size_t) ch].process (x, Type::highpass), Type::highpass);
            x *= sign;
            for (auto& b : bands)
                if (b.active)
                    x = b.filter.process (ch, x);
            io[ch][i] = x;
            level = juce::jmax (level, std::abs (x));
        }

        // Compressor, stereo linked.
        const auto amount = compressAmount.getCurrentValue();
        if (amount > 1.0e-4f || gainSmoother.getGainDb() < -1.0e-4f)
        {
            const auto inDb = rkfx::Dynamics::toDb (detector.process (level));
            const auto targetDb = rkfx::Dynamics::compressorCurve (inDb, thresholdFor (amount), ratioFor (amount), kKneeDb) - inDb;
            const auto gain = rkfx::Dynamics::toGain (gainSmoother.process (targetDb) + makeupFor (amount));
            for (int ch = 0; ch < numChannels; ++ch)
                io[ch][i] *= gain;
        }
        else
        {
            detector.process (level);
        }
    }
}
