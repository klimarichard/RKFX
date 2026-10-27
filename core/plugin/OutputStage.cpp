#include "OutputStage.h"

#include <cmath>

namespace rkfx
{

void OutputStage::prepare (double sampleRate)
{
    mix.reset (sampleRate, kParamSmoothingSeconds);
    gain.reset (sampleRate, kParamSmoothingSeconds);
    bypass.reset (sampleRate, kBypassFadeSeconds);
    reset();
}

void OutputStage::reset()
{
    mix.setCurrentAndTargetValue (mix.getTargetValue());
    gain.setCurrentAndTargetValue (gain.getTargetValue());
    bypass.setCurrentAndTargetValue (bypass.getTargetValue());
}

void OutputStage::setMix (float mix01) noexcept
{
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mix01));
}

void OutputStage::setOutputDecibels (float db) noexcept
{
    gain.setTargetValue (juce::Decibels::decibelsToGain (db, -200.0f));
}

void OutputStage::setBypassed (bool shouldBeBypassed) noexcept
{
    bypass.setTargetValue (shouldBeBypassed ? 1.0f : 0.0f);
}

bool OutputStage::wetNeeded() const noexcept
{
    return bypass.isSmoothing() || bypass.getTargetValue() < 1.0f;
}

void OutputStage::mixGains (float m, float& dryGain, float& wetGain) noexcept
{
    // Exact at the ends: cos(pi/2) is not exactly 0 in floating point.
    if (m >= 1.0f) { dryGain = 0.0f; wetGain = 1.0f; return; }
    if (m <= 0.0f) { dryGain = 1.0f; wetGain = 0.0f; return; }

    const auto angle = m * juce::MathConstants<float>::halfPi;
    dryGain = std::cos (angle);
    wetGain = std::sin (angle);
}

void OutputStage::process (const float* const* dry, float* const* io, int numChannels, int numSamples) noexcept
{
    const bool gliding = mix.isSmoothing() || gain.isSmoothing() || bypass.isSmoothing();

    if (! gliding)
    {
        const auto b = bypass.getTargetValue();

        if (b >= 1.0f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                juce::FloatVectorOperations::copy (io[ch], dry[ch], numSamples);
            return;
        }

        float dryGain, wetGain;
        mixGains (mix.getTargetValue(), dryGain, wetGain);
        const auto g = gain.getTargetValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (wetGain != 1.0f)
                juce::FloatVectorOperations::multiply (io[ch], wetGain, numSamples);
            if (dryGain != 0.0f)
                juce::FloatVectorOperations::addWithMultiply (io[ch], dry[ch], dryGain, numSamples);
            if (g != 1.0f)
                juce::FloatVectorOperations::multiply (io[ch], g, numSamples);
        }
        return;
    }

    // Something is gliding: per-sample path. Only runs for ~20 ms after a change.
    for (int i = 0; i < numSamples; ++i)
    {
        float dryGain, wetGain;
        mixGains (mix.getNextValue(), dryGain, wetGain);
        const auto g = gain.getNextValue();
        const auto b = bypass.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto d = dry[ch][i];
            const auto processed = g * (d * dryGain + io[ch][i] * wetGain);
            io[ch][i] = processed + b * (d - processed);
        }
    }
}

} // namespace rkfx
