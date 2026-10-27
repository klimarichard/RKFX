#include "ChorusEngine.h"

#include <cmath>

namespace
{
    constexpr double kTwoPi = 6.28318530717958647692;
}

void ChorusEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    const auto maxDelay = (int) std::ceil (kMaxDelayMs * (1.0f + kMaxDepthFraction) * 0.001 * sampleRate) + 4;
    for (auto& line : lines)
        line.prepare (maxDelay);
    for (auto& t : tilts)
        t.prepare (sampleRate);

    delaySamples.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    depth.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    width.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    fadeLength = juce::jmax (1, (int) (0.02 * sampleRate));

    setSettings (settings);
    reset();
}

void ChorusEngine::reset()
{
    for (auto& line : lines)
        line.reset();
    for (auto& t : tilts)
        t.reset();

    phase = 0.0;
    delaySamples.setCurrentAndTargetValue (delaySamples.getTargetValue());
    depth.setCurrentAndTargetValue (depth.getTargetValue());
    width.setCurrentAndTargetValue (width.getTargetValue());
    previousVoices = voices = juce::jlimit (1, kMaxVoices, settings.voices);
    fadeRemaining = 0;
}

void ChorusEngine::setSettings (const Settings& s)
{
    settings = s;

    delaySamples.setTargetValue (juce::jlimit (1.0f, kMaxDelayMs, s.delayMs) * 0.001f * (float) sampleRate);
    depth.setTargetValue (juce::jlimit (0.0f, 1.0f, s.depth));
    width.setTargetValue (juce::jlimit (0.0f, 1.0f, s.width));
    for (auto& t : tilts)
        t.setTone (juce::jlimit (-1.0f, 1.0f, s.tone));
}

float ChorusEngine::voicesOutput (int channel, int count, float delay, float modulation) const noexcept
{
    // Left and right run half a cycle apart; voices are spread evenly over the cycle.
    const auto channelOffset = channel == 0 ? 0.0 : 0.5;
    float sum = 0.0f;

    for (int v = 0; v < count; ++v)
    {
        const auto lfo = (float) std::sin (kTwoPi * (phase + channelOffset + (double) v / count));
        sum += lines[(size_t) channel].read (delay + modulation * lfo);
    }

    return sum / std::sqrt ((float) count);
}

void ChorusEngine::process (float* const* io, int numChannels, int numSamples, const rkfx::Transport& transport) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    // A voice count change crossfades from the old count to the new one.
    const auto wanted = juce::jlimit (1, kMaxVoices, settings.voices);
    if (wanted != voices && fadeRemaining == 0)
    {
        previousVoices = voices;
        voices = wanted;
        fadeRemaining = fadeLength;
    }

    double rate = settings.rateHz;
    if (settings.sync)
    {
        const auto beats = rkfx::SyncDivision::beats (settings.division);
        rate = transport.bpm / 60.0 / beats;
        if (transport.hasPosition && transport.playing)
            phase = transport.ppq / beats - std::floor (transport.ppq / beats);
    }
    const auto increment = rate / sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto delay = delaySamples.getNextValue();
        const auto modulation = depth.getNextValue() * kMaxDepthFraction * delay;
        const auto w = width.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            lines[(size_t) ch].write (io[ch][i]);

        float wet[kMaxChannels] {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto y = voicesOutput (ch, voices, delay, modulation);
            if (fadeRemaining > 0)
            {
                const auto t = (float) fadeRemaining / (float) fadeLength;
                y += t * (voicesOutput (ch, previousVoices, delay, modulation) - y);
            }
            wet[ch] = tilts[(size_t) ch].process (y);
        }

        if (numChannels == 2)
        {
            // Stereo width on the wet signal (mid/side).
            const auto mid = 0.5f * (wet[0] + wet[1]);
            const auto side = 0.5f * (wet[0] - wet[1]) * w;
            wet[0] = mid + side;
            wet[1] = mid - side;
        }

        for (int ch = 0; ch < numChannels; ++ch)
            io[ch][i] = wet[ch];

        if (fadeRemaining > 0)
            --fadeRemaining;

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
    }
}
