#include "FlangeEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

void FlangeEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    const auto maxDelay = (int) std::ceil (kMaxDelayMs * 0.001 * sampleRate) + 4;
    for (auto& line : lines)
        line.prepare (maxDelay);
    for (auto& dc : dcBlockers)
        dc.prepare (sampleRate);
    for (auto& lfo : lfos)
    {
        lfo.prepare (sampleRate);
        lfo.setShape (rkfx::LFO::Shape::sine);
    }

    manualSamples.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    depth.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    feedback.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void FlangeEngine::reset()
{
    for (auto& line : lines)
        line.reset();
    for (auto& dc : dcBlockers)
        dc.reset();

    lfos[0].reset();
    lfos[1].reset (settings.phaseDeg / 360.0);

    manualSamples.setCurrentAndTargetValue (manualSamples.getTargetValue());
    depth.setCurrentAndTargetValue (depth.getTargetValue());
    feedback.setCurrentAndTargetValue (feedback.getTargetValue());
}

void FlangeEngine::setSettings (const Settings& s)
{
    settings = s;
    manualSamples.setTargetValue (juce::jlimit (0.05f, 20.0f, s.manualMs) * 0.001f * (float) sampleRate);
    depth.setTargetValue (juce::jlimit (0.0f, 1.0f, s.depth));
    feedback.setTargetValue (juce::jlimit (-0.95f, 0.95f, s.feedback));
}

void FlangeEngine::process (float* const* io, int numChannels, int numSamples, const rkfx::Transport& transport) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    double rate = settings.rateHz;
    if (settings.sync)
    {
        const auto beats = rkfx::SyncDivision::beats (settings.division);
        rate = transport.bpm / 60.0 / beats;
        if (transport.hasPosition && transport.playing)
            lfos[0].syncTo (transport.ppq, beats);
    }
    for (auto& lfo : lfos)
        lfo.setRate (rate);
    lfos[1].syncTo (lfos[0].getPhase(), 1.0, settings.phaseDeg / 360.0);

    // Shortest delay: two samples (one for reading before writing, one for the interpolation).
    const auto maxDelay = (float) lines[0].getMaxDelay() - 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto manual = manualSamples.getNextValue();
        const auto d = depth.getNextValue();
        const auto fb = feedback.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto lfo = lfos[(size_t) ch].next();
            const auto delay = juce::jlimit (2.0f, maxDelay, manual * std::exp2 (kSweepOctaves * d * lfo));

            // Read before writing this sample, so the loop has its one-sample delay built in.
            auto& line = lines[(size_t) ch];
            const auto y = line.read (delay - 1.0f);

            const auto loop = rkfx::softClip (dcBlockers[(size_t) ch].process (y));
            line.write (io[ch][i] + fb * loop);

            io[ch][i] = y;
        }

        if (numChannels == 1)
            lfos[1].next();
    }
}
