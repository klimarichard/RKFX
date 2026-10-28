#include "SwellEngine.h"

#include "dsp/Dynamics.h"

#include <cmath>
#include <limits>

namespace
{
    constexpr float kLevelAttackMs = 1.0f, kLevelReleaseMs = 50.0f;
    constexpr float kOnsetFastMs = 3.0f, kOnsetSlowMs = 60.0f;
    constexpr double kGainSmoothingMs = 1.0;

    float powerDb (float p) noexcept { return 10.0f * std::log10 (p + 1.0e-12f); }
}

float SwellEngine::curveGain (Curve c, float p) noexcept
{
    p = juce::jlimit (0.0f, 1.0f, p);
    switch (c)
    {
        case Curve::linear:      return p;
        case Curve::exponential: return p <= 0.0f ? 0.0f : std::pow (10.0f, -3.0f * (1.0f - p));   // -60 dB -> 0 dB, straight in dB
        case Curve::sCurve:      return p * p * (3.0f - 2.0f * p);
    }
    return p;
}

float SwellEngine::onsetThresholdDb (float retrigger) noexcept
{
    if (retrigger <= 0.0f)
        return std::numeric_limits<float>::infinity();
    // 100% -> 3 dB jump (very sensitive); towards 0% -> about 18 dB (only hard attacks).
    return 3.0f + 15.0f * (1.0f - juce::jmin (1.0f, retrigger));
}

void SwellEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    gainSmoothing = (float) std::exp (-1.0 / (kGainSmoothingMs * 0.001 * sampleRate));
    dropStep = (float) (1.0 / (kDropMs * 0.001 * sampleRate));

    for (auto& v : voices)
    {
        v.level.prepare (sampleRate);
        v.level.setTimes (kLevelAttackMs, kLevelReleaseMs);
        v.onsetFast.prepare (sampleRate);
        v.onsetFast.setTimes (kOnsetFastMs, kOnsetFastMs);
        v.onsetSlow.prepare (sampleRate);
        v.onsetSlow.setTimes (kOnsetSlowMs, kOnsetSlowMs);
    }

    setSettings (settings);
    reset();
}

void SwellEngine::reset()
{
    for (auto& v : voices)
    {
        v.level.reset();
        v.onsetFast.reset();
        v.onsetSlow.reset();
        v.sounding = v.dropping = false;
        v.armed = true;
        v.position = 0.0f;
        v.gain = 0.0f;
    }
}

void SwellEngine::setSettings (const Settings& s)
{
    settings = s;
    riseStep = (float) (1.0 / (juce::jlimit (1.0f, 10000.0f, s.timeMs) * 0.001 * sampleRate));
}

float SwellEngine::advance (Voice& v, float detector) noexcept
{
    const auto levelDb = rkfx::Dynamics::toDb (v.level.process (detector));
    const auto power = detector * detector;
    const auto jumpDb = powerDb (v.onsetFast.process (power)) - powerDb (v.onsetSlow.process (power));

    // Silence and sound, with hysteresis.
    if (v.sounding && levelDb < settings.thresholdDb - kHysteresisDb)
    {
        v.sounding = false;
        v.dropping = true;   // fade out quickly; the next sound swells from silence
    }
    else if (! v.sounding && levelDb >= settings.thresholdDb)
    {
        v.sounding = true;
        v.armed = false;
    }

    // Retrigger on a new attack within a phrase (re-armed once the jump has passed).
    const auto onsetDb = onsetThresholdDb (settings.retrigger);
    if (v.sounding)
    {
        if (v.armed && jumpDb > onsetDb && v.position > 0.0f)
        {
            v.dropping = true;
            v.armed = false;
        }
        else if (! v.armed && jumpDb < onsetDb * 0.5f)
        {
            v.armed = true;
        }
    }

    // Position: drop quickly to 0, then swell up while sounding.
    if (v.dropping)
    {
        v.position -= dropStep;
        if (v.position <= 0.0f)
        {
            v.position = 0.0f;
            v.dropping = false;
        }
    }
    else if (v.sounding)
    {
        v.position = juce::jmin (1.0f, v.position + riseStep);
    }

    const auto target = curveGain (settings.curve, v.position);
    v.gain = target + gainSmoothing * (v.gain - target);
    return v.gain;
}

void SwellEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        if (settings.link)
        {
            float detector = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                detector = juce::jmax (detector, std::abs (io[ch][i]));

            const auto g = advance (voices[0], detector);
            for (int ch = 0; ch < numChannels; ++ch)
                io[ch][i] *= g;
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
                io[ch][i] *= advance (voices[(size_t) ch], std::abs (io[ch][i]));
        }
    }
}
