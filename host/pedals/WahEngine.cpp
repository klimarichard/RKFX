#include "WahEngine.h"

#include <cmath>

namespace
{
    constexpr int kUpdateInterval = 16;     // samples between coefficient updates
    constexpr float kHeelRest = 0.02f;      // "at the heel" for auto engage
    constexpr float kMoveToEngage = 0.03f;
}

float WahEngine::frequencyFor (const Settings& s, float position)
{
    const auto lo = juce::jmin (s.lowHz, s.highHz), hi = juce::jmax (s.lowHz, s.highHz);
    const auto t = std::pow (juce::jlimit (0.0f, 1.0f, position), 1.4f);
    return lo * std::pow (hi / lo, t);
}

void WahEngine::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    for (auto& c : channels)
    {
        c.peak.prepare (sampleRate);
        c.formant.prepare (sampleRate);
        c.lows.prepare (sampleRate);
        c.lows.setCutoff (150.0f);
    }

    positionCoeff = (float) (1.0 - std::exp (-1.0 / (0.015 * sampleRate)));   // 15 ms
    engageStep = (float) (1.0 / (0.01 * sampleRate));                          // 10 ms crossfade
}

void WahEngine::reset (const Settings& s)
{
    for (auto& c : channels)
    {
        c.peak.reset();
        c.formant.reset();
        c.lows.reset();
    }

    smoothedPosition = s.position;
    engaged = ! s.autoEngage || s.position > kHeelRest;
    engageAmount = engaged ? 1.0f : 0.0f;
    restPosition = s.position;
    heelSamples = 0;
}

void WahEngine::updateEngage (const Settings& s, int numSamples)
{
    if (! s.autoEngage)
    {
        engaged = true;
        return;
    }

    if (! engaged)
    {
        // Moving the pedal off its rest switches it on.
        if (std::abs (s.position - restPosition) > kMoveToEngage)
        {
            engaged = true;
            heelSamples = 0;
        }
        return;
    }

    // Resting at the heel for half a second switches it off.
    heelSamples = s.position <= kHeelRest ? heelSamples + numSamples : 0;
    if (heelSamples > (int) (0.5 * sampleRate))
    {
        engaged = false;
        restPosition = s.position;
    }
}

void WahEngine::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (buffer.getNumChannels(), (int) channels.size());
    updateEngage (s, numSamples);

    const auto q = s.character == Character::classic ? 2.0f + 8.0f * s.resonance
                 : s.character == Character::vocal   ? 1.0f + 3.0f * s.resonance
                                                     : 1.5f + 6.0f * s.resonance;
    const auto shift = s.character == Character::bass ? 0.5f : 1.0f;

    for (int start = 0; start < numSamples; start += kUpdateInterval)
    {
        const auto length = juce::jmin (kUpdateInterval, numSamples - start);

        // Position smoothing, then one coefficient update per few samples.
        for (int i = 0; i < length; ++i)
            smoothedPosition += positionCoeff * (s.position - smoothedPosition);

        const auto hz = frequencyFor (s, smoothedPosition) * shift;
        const auto g = rkfx::StateVariableFilter::gFor (hz, sampleRate);
        const auto gFormant = rkfx::StateVariableFilter::gFor (juce::jmin (hz * 2.5f, 0.45f * (float) sampleRate), sampleRate);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            c.peak.setCoefficients (g, 1.0f / q);
            c.formant.setCoefficients (gFormant, 1.0f / (q * 0.8f));
        }

        auto amount = engageAmount;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            auto* x = buffer.getWritePointer (ch, start);
            amount = engageAmount;

            for (int i = 0; i < length; ++i)
            {
                const auto dry = x[i];
                auto wet = c.peak.processNormalisedBandpass (dry);

                if (s.character == Character::vocal)
                    wet = 0.65f * wet + 0.45f * c.formant.processNormalisedBandpass (dry);
                else if (s.character == Character::bass)
                    wet += c.lows.process (dry, rkfx::StateVariableFilter::Type::lowpass);

                amount = engaged ? juce::jmin (1.0f, amount + engageStep) : juce::jmax (0.0f, amount - engageStep);
                x[i] = dry + amount * (wet - dry);
            }
        }
        engageAmount = amount;
    }
}
