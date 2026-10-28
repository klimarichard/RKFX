#include "LimitEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

using rkfx::Dynamics::toDb;
using rkfx::Dynamics::toGain;

float LimitEngine::clip (float x, float knee, float ceil) noexcept
{
    const auto magnitude = std::abs (x);
    if (magnitude <= knee)
        return x;

    // Above the knee: soft-clip the excess into the space left below the ceiling.
    const auto room = ceil - knee;
    const auto y = knee + room * rkfx::softClip ((magnitude - knee) / room);
    return x < 0.0f ? -y : y;
}

void LimitEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& s : smoothers)
        s.prepare (sampleRate);

    inputGain.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    ceiling.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    link.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void LimitEngine::reset()
{
    for (auto& s : smoothers)
        s.reset();
    oversampler.reset();
    factor = settings.oversampling;

    inputGain.setCurrentAndTargetValue (inputGain.getTargetValue());
    ceiling.setCurrentAndTargetValue (ceiling.getTargetValue());
    link.setCurrentAndTargetValue (link.getTargetValue());
    peakReduction = 0.0f;
}

void LimitEngine::setSettings (const Settings& s)
{
    settings = s;
    settings.oversampling = s.oversampling >= 4 ? 4 : s.oversampling >= 2 ? 2 : 1;

    inputGain.setTargetValue (toGain (juce::jlimit (0.0f, 36.0f, s.inputDb)));
    ceiling.setTargetValue (toGain (juce::jlimit (-24.0f, 0.0f, s.ceilingDb)));
    link.setTargetValue (juce::jlimit (0.0f, 1.0f, s.link));

    for (auto& sm : smoothers)
    {
        sm.setTimes (0.0f, juce::jlimit (1.0f, 2000.0f, s.releaseMs));   // instant attack
        sm.setAutoRelease (s.autoRelease);
    }
}

float LimitEngine::takeGainReductionDb() noexcept
{
    const auto r = peakReduction;
    peakReduction = 0.0f;
    return r;
}

void LimitEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    if (settings.oversampling != factor)
    {
        factor = settings.oversampling;
        oversampler.reset();
    }

    // The ceiling only moves slowly; the clipper uses the block's value.
    const auto ceil = ceiling.getCurrentValue();
    const auto knee = ceil * toGain (-kHeadroomDb);
    const auto thresholdDb = toDb (knee);

    // Stage 1: input gain and the peak limiter.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = inputGain.getNextValue();
        const auto l = link.getNextValue();
        ceiling.getNextValue();

        float levelDb[kMaxChannels] {};
        auto loudest = rkfx::Dynamics::kSilenceDb;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            io[ch][i] *= g;
            levelDb[ch] = toDb (std::abs (io[ch][i]));
            loudest = juce::jmax (loudest, levelDb[ch]);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto level = l * loudest + (1.0f - l) * levelDb[ch];
            const auto target = juce::jmin (0.0f, thresholdDb - level);
            const auto gainDb = smoothers[(size_t) ch].process (target);
            peakReduction = juce::jmin (peakReduction, gainDb);
            if (gainDb < 0.0f)
                io[ch][i] *= toGain (gainDb);
        }
    }

    // Stage 2: the safety clipper (oversampled if enabled), then the hard ceiling.
    oversampler.process (io, numChannels, numSamples, factor, [knee, ceil] (int, int, float x) { return clip (x, knee, ceil); });

    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            io[ch][i] = juce::jlimit (-ceil, ceil, io[ch][i]);
}
