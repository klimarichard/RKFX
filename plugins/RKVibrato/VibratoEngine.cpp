#include "VibratoEngine.h"

#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr float kMinRateHz = 0.05f;

    float powerDb (float p) noexcept { return 10.0f * std::log10 (p + 1.0e-12f); }
}

double VibratoEngine::shapeIntegral (Shape shape, double p) noexcept
{
    p -= std::floor (p);

    if (shape == Shape::sine)
        return (1.0 - std::cos (2.0 * kPi * p)) / (2.0 * kPi) - 0.5 / kPi;   // integral of sin, centred

    // Integral of a triangle (0 -> 1 -> -1 -> 0 over the cycle), centred.
    double f;
    if (p < 0.25)      f = 2.0 * p * p;
    else if (p < 0.75) f = 2.0 * p - 2.0 * p * p - 0.25;
    else               f = 2.0 * (1.0 - p) * (1.0 - p);
    return f - 0.125;
}

double VibratoEngine::integralSwing (Shape shape) noexcept
{
    return shape == Shape::sine ? 1.0 / kPi : 0.25;
}

void VibratoEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    // Largest swing: 1 semitone at the slowest rate.
    const auto maxSwingSeconds = (std::pow (2.0, kMaxSemitones / 12.0) - 1.0) / kMinRateHz * integralSwing (Shape::sine);
    for (auto& line : lines)
        line.prepare ((int) std::ceil ((maxSwingSeconds + 0.002) * sampleRate) + 8);

    onsetFast.prepare (sampleRate);
    onsetFast.setTimes (3.0f, 3.0f);
    onsetSlow.prepare (sampleRate);
    onsetSlow.setTimes (60.0f, 60.0f);

    ratioSwing.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    rate.reset (sampleRate, 0.05);

    setSettings (settings);
    reset();
}

void VibratoEngine::reset()
{
    for (auto& line : lines)
        line.reset();
    phase = 0.0;
    onsetFast.reset();
    onsetSlow.reset();
    armed = true;
    rise = 1.0f;
    ratioSwing.setCurrentAndTargetValue (ratioSwing.getTargetValue());
    rate.setCurrentAndTargetValue (rate.getTargetValue());
}

void VibratoEngine::setSettings (const Settings& s)
{
    settings = s;
    ratioSwing.setTargetValue ((float) (std::pow (2.0, juce::jlimit (0.0f, 1.0f, s.depth) * kMaxSemitones / 12.0) - 1.0));
    riseStep = s.riseSeconds <= 0.0f ? 1.0f : (float) (1.0 / (s.riseSeconds * sampleRate));
}

void VibratoEngine::process (float* const* io, int numChannels, int numSamples, const rkfx::Transport& transport) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    double hz = settings.rateHz;
    if (settings.sync)
    {
        const auto beats = rkfx::SyncDivision::beats (settings.division);
        hz = transport.bpm / 60.0 / beats;
        if (transport.hasPosition && transport.playing)
            phase = transport.ppq / beats - std::floor (transport.ppq / beats);
    }
    rate.setTargetValue ((float) juce::jlimit ((double) kMinRateHz, 50.0, hz));

    const auto swingUnit = integralSwing (settings.shape);
    const auto phaseOffset = settings.phaseDeg / 360.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto r = ratioSwing.getNextValue();
        const auto f = rate.getNextValue();

        // Rise: a new note (a sudden jump in level) restarts the depth fade-in.
        float detector = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            detector = juce::jmax (detector, std::abs (io[ch][i]));
        const auto p = detector * detector;
        const auto jump = powerDb (onsetFast.process (p)) - powerDb (onsetSlow.process (p));
        if (armed && jump > kOnsetJumpDb && settings.riseSeconds > 0.0f)
        {
            rise = 0.0f;
            armed = false;
        }
        else if (! armed && jump < kOnsetJumpDb * 0.5f)
        {
            armed = true;
        }
        rise = juce::jmin (1.0f, rise + riseStep);

        // Delay in samples: centre (enough for the full swing) plus the shape's integral.
        const auto swingSamples = (double) r / f * sampleRate;
        const auto centre = kMinDelayMs * 0.001 * sampleRate + 0.5 * swingSamples * swingUnit;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& line = lines[(size_t) ch];
            line.write (io[ch][i]);
            const auto lfo = shapeIntegral (settings.shape, phase + (ch == 0 ? 0.0 : phaseOffset));
            io[ch][i] = line.read ((float) (centre - rise * swingSamples * lfo));
        }

        phase += f / sampleRate;
        if (phase >= 1.0)
            phase -= 1.0;
    }
}
