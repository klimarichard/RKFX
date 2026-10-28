#include "TransientEngine.h"

#include "dsp/Dynamics.h"

#include <cmath>

namespace
{
    // Power averages (equal rise and fall), so on a steady sound each pair settles
    // on the same value and leaves it alone; they only differ on hits and decays.
    constexpr float kAttackFastMs = 3.0f;
    constexpr float kSustainFastMs = 15.0f;
    constexpr double kGainSmoothingMs = 4.0;

    // dB of gain per dB of attack/sustain difference at 100%: sets how strong
    // each control is (the averages differ by only a few dB on typical hits;
    // the attack difference is the smaller one, so it gets more).
    constexpr float kAttackSensitivity = 5.0f;
    constexpr float kSustainSensitivity = 2.5f;

    float powerDb (float power) noexcept { return 10.0f * std::log10 (power + 1.0e-12f); }
}

void TransientEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    gainSmoothing = (float) std::exp (-1.0 / (kGainSmoothingMs * 0.001 * sampleRate));

    for (auto& d : detectors)
        for (auto* f : { &d.attackFast, &d.attackSlow, &d.sustainFast, &d.sustainSlow })
            f->prepare (sampleRate);
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        highPass[(size_t) ch].prepare (sampleRate);
        lowPass[(size_t) ch].prepare (sampleRate);
    }

    attack.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    sustain.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    listen.reset (sampleRate, rkfx::kBypassFadeSeconds);

    setSettings (settings);
    reset();
}

void TransientEngine::reset()
{
    for (auto& d : detectors)
    {
        for (auto* f : { &d.attackFast, &d.attackSlow, &d.sustainFast, &d.sustainSlow })
            f->reset();
        d.smoothedGainDb = 0.0f;
    }
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        highPass[(size_t) ch].reset();
        lowPass[(size_t) ch].reset();
    }

    attack.setCurrentAndTargetValue (attack.getTargetValue());
    sustain.setCurrentAndTargetValue (sustain.getTargetValue());
    listen.setCurrentAndTargetValue (listen.getTargetValue());
}

void TransientEngine::configureFollowers()
{
    const auto attackTime = juce::jlimit (1.0f, 100.0f, settings.attackTimeMs);
    const auto sustainTime = juce::jlimit (10.0f, 2000.0f, settings.sustainTimeMs);

    for (auto& d : detectors)
    {
        d.attackFast.setTimes (kAttackFastMs, kAttackFastMs);
        d.attackSlow.setTimes (attackTime, attackTime);
        d.sustainFast.setTimes (kSustainFastMs, kSustainFastMs);
        d.sustainSlow.setTimes (sustainTime, sustainTime);
    }
}

void TransientEngine::setSettings (const Settings& s)
{
    settings = s;
    attack.setTargetValue (juce::jlimit (-1.0f, 1.0f, s.attack));
    sustain.setTargetValue (juce::jlimit (-1.0f, 1.0f, s.sustain));
    listen.setTargetValue (s.listen ? 1.0f : 0.0f);
    configureFollowers();
}

float TransientEngine::gainDb (Detector& d, float level, float attackAmount, float sustainAmount) noexcept
{
    const auto power = level * level;
    const auto attackFast = d.attackFast.process (power);
    const auto attackSlow = d.attackSlow.process (power);
    const auto sustainFast = d.sustainFast.process (power);
    const auto sustainSlow = d.sustainSlow.process (power);

    // How far the sound is in its attack / its sustain, in dB (0 = not at all).
    const auto inAttack = juce::jmax (0.0f, powerDb (attackFast) - powerDb (attackSlow));
    const auto inSustain = juce::jmax (0.0f, powerDb (sustainSlow) - powerDb (sustainFast));

    const auto target = juce::jlimit (-kMaxGainDb, kMaxGainDb, kAttackSensitivity * attackAmount * inAttack + kSustainSensitivity * sustainAmount * inSustain);
    d.smoothedGainDb = target + gainSmoothing * (d.smoothedGainDb - target);
    return d.smoothedGainDb;
}

void TransientEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    const auto highPassOn = settings.focusHighPassHz > 0.0f;
    const auto lowPassOn = settings.focusLowPassHz > 0.0f;
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        if (highPassOn && ! highPassRunning) highPass[(size_t) ch].reset();
        if (lowPassOn && ! lowPassRunning)   lowPass[(size_t) ch].reset();
        if (highPassOn) highPass[(size_t) ch].setCutoff (settings.focusHighPassHz);
        if (lowPassOn)  lowPass[(size_t) ch].setCutoff (settings.focusLowPassHz);
    }
    highPassRunning = highPassOn;
    lowPassRunning = lowPassOn;

    const auto idle = ! attack.isSmoothing() && ! sustain.isSmoothing() && ! listen.isSmoothing()
                   && std::abs (attack.getTargetValue()) < 1.0e-6f && std::abs (sustain.getTargetValue()) < 1.0e-6f
                   && listen.getTargetValue() < 0.5f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto a = attack.getNextValue();
        const auto s = sustain.getNextValue();
        const auto l = listen.getNextValue();

        float focused[kMaxChannels] {};
        float loudest = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto x = io[ch][i];
            if (highPassOn) x = highPass[(size_t) ch].process (x, rkfx::StateVariableFilter::Type::highpass);
            if (lowPassOn)  x = lowPass[(size_t) ch].process (x, rkfx::StateVariableFilter::Type::lowpass);
            focused[ch] = x;
            loudest = juce::jmax (loudest, std::abs (x));
        }

        // Shaping off: keep the followers running (no jump when it's turned up), signal untouched.
        const auto linkedGain = settings.link ? gainDb (detectors[0], loudest, a, s) : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto g = settings.link ? linkedGain : gainDb (detectors[(size_t) ch], std::abs (focused[ch]), a, s);
            if (idle)
                continue;

            auto y = io[ch][i] * rkfx::Dynamics::toGain (g);
            if (l > 0.0f)
                y += l * (focused[ch] - y);
            io[ch][i] = y;
        }
    }
}
