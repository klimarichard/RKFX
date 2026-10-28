#include "DeEssEngine.h"

#include <cmath>

namespace
{
    // Fixed, fast timing tuned for sibilance.
    constexpr float kDetectAttackMs = 0.5f, kDetectReleaseMs = 20.0f;
    constexpr float kGainAttackMs = 0.5f, kGainReleaseMs = 40.0f;
}

float DeEssEngine::reductionDb (const Settings& s, float bandDb, float fullDb) noexcept
{
    const auto over = s.detection == Detection::relative
                    ? (bandDb - fullDb) + kRelativeOffsetDb - s.relativeThresholdDb
                    : bandDb - s.thresholdDb;

    if (over <= 0.0f)
        return 0.0f;

    return -juce::jmin (juce::jmax (0.0f, s.rangeDb), over * (1.0f - 1.0f / kRatio));
}

void DeEssEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& c : channels)
    {
        c.detector.prepare (sampleRate);
        c.split.prepare (sampleRate);
        c.bandLevel.prepare (sampleRate);
        c.fullLevel.prepare (sampleRate);
        c.bandLevel.setTimes (kDetectAttackMs, kDetectReleaseMs);
        c.fullLevel.setTimes (kDetectAttackMs, kDetectReleaseMs);
        c.smoother.prepare (sampleRate);
        c.smoother.setTimes (kGainAttackMs, kGainReleaseMs);
    }

    listen.reset (sampleRate, rkfx::kBypassFadeSeconds);
    wide.reset (sampleRate, rkfx::kBypassFadeSeconds);

    setSettings (settings);
    reset();
}

void DeEssEngine::reset()
{
    for (auto& c : channels)
    {
        c.detector.reset();
        c.split.reset();
        c.bandLevel.reset();
        c.fullLevel.reset();
        c.smoother.reset();
    }
    listen.setCurrentAndTargetValue (listen.getTargetValue());
    wide.setCurrentAndTargetValue (wide.getTargetValue());
    peakReduction = 0.0f;
}

void DeEssEngine::setSettings (const Settings& s)
{
    settings = s;
    const auto hz = juce::jlimit (1000.0f, 16000.0f, s.frequencyHz);

    for (auto& c : channels)
    {
        c.detector.setCutoff (hz);
        c.split.setCutoff (hz);
    }

    listen.setTargetValue (s.listen ? 1.0f : 0.0f);
    wide.setTargetValue (s.mode == Mode::wide ? 1.0f : 0.0f);
}

float DeEssEngine::takeGainReductionDb() noexcept
{
    const auto r = peakReduction;
    peakReduction = 0.0f;
    return r;
}

void DeEssEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    using rkfx::Dynamics::toDb;
    using rkfx::Dynamics::toGain;

    numChannels = juce::jmin (numChannels, kMaxChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto l = listen.getNextValue();
        const auto w = wide.getNextValue();

        float band[kMaxChannels] {}, bandDb[kMaxChannels] {}, fullDb[kMaxChannels] {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const auto x = io[ch][i];
            band[ch] = c.detector.process (x, rkfx::StateVariableFilter::Type::highpass);
            bandDb[ch] = toDb (c.bandLevel.process (band[ch]));
            fullDb[ch] = toDb (c.fullLevel.process (x));
        }

        // Linked: the channel asking for the most reduction decides for both.
        float target[kMaxChannels] {};
        auto linkedTarget = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            target[ch] = reductionDb (settings, bandDb[ch], fullDb[ch]);
            linkedTarget = juce::jmin (linkedTarget, target[ch]);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const auto gainDb = c.smoother.process (settings.link ? linkedTarget : target[ch]);
            peakReduction = juce::jmin (peakReduction, gainDb);

            const auto x = io[ch][i];
            auto y = x;

            if (gainDb < -1.0e-3f)   // less than 0.001 dB: treat as no reduction
            {
                const auto g = toGain (gainDb);
                // Split: reduce only the band above the frequency (high = x - low-pass).
                const auto low = c.split.process (x, rkfx::StateVariableFilter::Type::lowpass);
                const auto splitOut = low + g * (x - low);
                y = splitOut + w * (x * g - splitOut);
            }
            else
            {
                c.split.process (x, rkfx::StateVariableFilter::Type::lowpass);   // keep the filter state current
            }

            if (l > 0.0f)
                y += l * (band[ch] - y);
            io[ch][i] = y;
        }
    }
}
