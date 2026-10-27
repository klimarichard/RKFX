#include "DoubleEngine.h"

#include <cmath>

void DoubleEngine::panGains (float pan, float& left, float& right) noexcept
{
    left = juce::jmin (1.0f, 1.0f - pan);
    right = juce::jmin (1.0f, 1.0f + pan);
}

void DoubleEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    const auto maxDelay = (int) std::ceil ((kMaxDelayMs + kMaxDriftMs) * 0.001 * sampleRate) + 4;
    for (auto& line : lines)
        line.prepare (maxDelay);
    for (auto& t : tilts)
        t.prepare (sampleRate);
    drift.prepare (sampleRate);

    delaySamples.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    driftSamples.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    spread.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    level.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void DoubleEngine::reset()
{
    for (auto& line : lines)
        line.reset();
    for (auto& t : tilts)
        t.reset();
    drift.reset();

    delaySamples.setCurrentAndTargetValue (delaySamples.getTargetValue());
    driftSamples.setCurrentAndTargetValue (driftSamples.getTargetValue());
    spread.setCurrentAndTargetValue (spread.getTargetValue());
    level.setCurrentAndTargetValue (level.getTargetValue());
}

void DoubleEngine::setSettings (const Settings& s)
{
    settings = s;

    const auto msToSamples = 0.001f * (float) sampleRate;
    delaySamples.setTargetValue (juce::jlimit (kMaxDriftMs * 2.0f, kMaxDelayMs, s.delayMs) * msToSamples);
    driftSamples.setTargetValue (juce::jlimit (0.0f, 1.0f, s.drift) * kMaxDriftMs * msToSamples);
    spread.setTargetValue (juce::jlimit (0.0f, 1.0f, s.spread));
    level.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-60.0f, 0.0f, s.levelDb)));
    drift.setRate (juce::jlimit (0.01f, 20.0f, s.driftHz));

    for (auto& t : tilts)
        t.setTone (juce::jlimit (-1.0f, 1.0f, s.tone));
}

void DoubleEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        // Base delay >= 10 ms and drift <= 5 ms: the double always lags.
        const auto delay = delaySamples.getNextValue() + driftSamples.getNextValue() * drift.next();
        const auto gain = level.getNextValue();
        const auto s = spread.getNextValue();

        float dry[kMaxChannels] {}, dbl[kMaxChannels] {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            dry[ch] = io[ch][i];
            lines[(size_t) ch].write (dry[ch]);
            dbl[ch] = gain * tilts[(size_t) ch].process (lines[(size_t) ch].read (delay));
        }

        if (numChannels == 2)
        {
            float dryL, dryR, dblL, dblR;
            panGains (-s, dryL, dryR);
            panGains (s, dblL, dblR);
            io[0][i] = dry[0] * dryL + dbl[0] * dblL;
            io[1][i] = dry[1] * dryR + dbl[1] * dblR;
        }
        else if (numChannels == 1)
        {
            io[0][i] = dry[0] + dbl[0];
        }
    }
}
