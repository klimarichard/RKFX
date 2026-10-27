#include "SaturateEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

namespace
{
    constexpr float kTubeBias = 0.25f;
    constexpr double kEmphasisHz = 200.0;     // transformer: lows below this driven harder
    constexpr float kEmphasisAmount = 1.0f;   // +6 dB on the lows into the shaper
    constexpr double kTapeRoundingHz = 12000.0;

    /** Slope of the soft clipper at the tube bias point, to keep unity small-signal gain. */
    float tubeSlope()
    {
        const auto h = 1.0e-3f;
        return (rkfx::softClip (kTubeBias + h) - rkfx::softClip (kTubeBias - h)) / (2.0f * h);
    }
}

float SaturateEngine::shape (Character c, float x) noexcept
{
    switch (c)
    {
        case Character::tape:
            return rkfx::softClip (x);

        case Character::tube:
        {
            static const auto slope = tubeSlope();
            return (rkfx::softClip (x + kTubeBias) - rkfx::softClip (kTubeBias)) / slope;
        }

        case Character::transformer:
        {
            // Cubic with a firmer knee: unity slope at 0, flat at +/-1 from |x| = 1.5.
            const auto u = juce::jlimit (-1.0f, 1.0f, x / 1.5f);
            return 1.5f * u - 0.5f * u * u * u;
        }
    }
    return x;
}

void SaturateEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    emphasisCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * kEmphasisHz / sampleRate);
    tapeCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * juce::jmin (kTapeRoundingHz, 0.4 * sampleRate) / sampleRate);
    fadeLength = juce::jmax (1, (int) (0.01 * sampleRate));

    for (auto& c : channels)
    {
        c.dc.prepare (sampleRate);
        c.tilt.prepare (sampleRate);
    }

    drive.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    outputGain.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    emphasis.reset (sampleRate, rkfx::kBypassFadeSeconds);
    tapeAmount.reset (sampleRate, rkfx::kBypassFadeSeconds);

    setSettings (settings);
    reset();
}

void SaturateEngine::reset()
{
    for (auto& c : channels)
    {
        c.emphasisLow = c.deemphasisX1 = c.deemphasisY1 = c.tapeLow = 0.0f;
        c.dc.reset();
        c.tilt.reset();
    }

    oversampler.reset();
    factor = settings.oversampling;
    previousCharacter = character = settings.character;
    fadeRemaining = 0;

    drive.setCurrentAndTargetValue (drive.getTargetValue());
    outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
    emphasis.setCurrentAndTargetValue (emphasis.getTargetValue());
    tapeAmount.setCurrentAndTargetValue (tapeAmount.getTargetValue());
}

void SaturateEngine::setSettings (const Settings& s)
{
    settings = s;
    settings.oversampling = s.oversampling >= 4 ? 4 : s.oversampling >= 2 ? 2 : 1;

    const auto driveDb = juce::jlimit (0.0f, 36.0f, s.driveDb);
    drive.setTargetValue (juce::Decibels::decibelsToGain (driveDb));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (s.autoGain ? autoGainDb (driveDb) : 0.0f));
    emphasis.setTargetValue (s.character == Character::transformer ? kEmphasisAmount : 0.0f);
    tapeAmount.setTargetValue (s.character == Character::tape ? 1.0f : 0.0f);

    for (auto& c : channels)
        c.tilt.setTone (juce::jlimit (-1.0f, 1.0f, s.tone));
}

void SaturateEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    // Oversampling change (setup only): restart the resampling filters.
    if (settings.oversampling != factor)
    {
        factor = settings.oversampling;
        oversampler.reset();
    }

    // Character change: crossfade the old curve to the new one.
    if (settings.character != character && fadeRemaining == 0)
    {
        previousCharacter = character;
        character = settings.character;
        fadeRemaining = fadeLength;
    }

    // Emphasis amount: constant over the block, so the de-emphasis is the exact inverse.
    const auto e = emphasis.skip (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = drive.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const auto x = io[ch][i] * g;

            // Transformer pre-emphasis: x + e * lowpass (x).
            c.emphasisLow = x + emphasisCoef * (c.emphasisLow - x);
            io[ch][i] = x + e * c.emphasisLow;
        }
    }

    // The curve, oversampled; during a character change, old and new curves crossfade.
    const auto fadeStart = fadeRemaining;
    const auto oldCharacter = previousCharacter, newCharacter = character;

    oversampler.process (io, numChannels, numSamples, factor, [&] (int, int index, float x)
    {
        const auto newY = shape (newCharacter, x);
        const auto remaining = fadeStart - index;
        if (remaining <= 0)
            return newY;

        const auto oldY = shape (oldCharacter, x);
        return oldY + (1.0f - (float) remaining / (float) fadeLength) * (newY - oldY);
    });
    fadeRemaining = juce::jmax (0, fadeRemaining - numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto tape = tapeAmount.getNextValue();
        const auto out = outputGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const auto x = io[ch][i];

            // Exact inverse of the pre-emphasis: (1 - c z^-1) / ((1 + e (1 - c)) - c z^-1).
            const auto y = (x - emphasisCoef * c.deemphasisX1 + emphasisCoef * c.deemphasisY1) / (1.0f + e * (1.0f - emphasisCoef));
            c.deemphasisX1 = x;
            c.deemphasisY1 = y;

            auto z = c.dc.process (y);
            c.tapeLow = z + tapeCoef * (c.tapeLow - z);
            z += tape * (c.tapeLow - z);

            io[ch][i] = out * c.tilt.process (z);
        }
    }
}
