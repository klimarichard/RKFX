#include "TremoloEngine.h"

#include <cmath>

void TremoloEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& lfo : lfos)
        lfo.prepare (sampleRate);
    for (auto& x : crossovers)
        x.prepare (sampleRate);

    depth.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    harmonicMix.reset (sampleRate, rkfx::kBypassFadeSeconds);

    setSettings (settings);
    reset();
}

void TremoloEngine::reset()
{
    lfos[0].reset();
    lfos[1].reset (settings.phaseDeg / 360.0);
    for (auto& x : crossovers)
        x.reset();

    depth.setCurrentAndTargetValue (depth.getTargetValue());
    harmonicMix.setCurrentAndTargetValue (harmonicMix.getTargetValue());
    crossoverRunning = settings.mode == Mode::harmonic;
    slewed.fill (1.0f);
}

void TremoloEngine::setSettings (const Settings& s)
{
    settings = s;
    depth.setTargetValue (juce::jlimit (0.0f, 1.0f, s.depth));
    harmonicMix.setTargetValue (s.mode == Mode::harmonic ? 1.0f : 0.0f);

    for (auto& lfo : lfos)
        lfo.setShape (s.shape);
    for (auto& x : crossovers)
        x.setFrequency (juce::jlimit (20.0f, 20000.0f, s.crossoverHz));
}

float TremoloEngine::slewCoefficient (double cycleHz) const noexcept
{
    // Always at least ~1 ms; the smoothing control adds up to 1/8 of a cycle.
    const auto seconds = 0.001 + (double) settings.smooth * 0.125 / juce::jmax (0.01, cycleHz);
    return (float) std::exp (-1.0 / (seconds * sampleRate));
}

void TremoloEngine::process (float* const* io, int numChannels, int numSamples, const rkfx::Transport& transport) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    // Rate: free, or tempo synced and locked to the host's beat.
    double cycleHz = settings.rateHz;
    if (settings.sync)
    {
        const auto beats = rkfx::SyncDivision::beats (settings.division);
        cycleHz = transport.bpm / 60.0 / beats;
        if (transport.hasPosition && transport.playing)
            lfos[0].syncTo (transport.ppq, beats);
    }
    for (auto& lfo : lfos)
        lfo.setRate (cycleHz);
    lfos[1].syncTo (lfos[0].getPhase(), 1.0, settings.phaseDeg / 360.0);

    const auto slew = slewCoefficient (cycleHz);

    // The crossover only runs in harmonic mode (and while fading out of it).
    const auto needCrossover = settings.mode == Mode::harmonic || harmonicMix.isSmoothing();
    if (needCrossover && ! crossoverRunning)
        for (auto& x : crossovers)
            x.reset();
    crossoverRunning = needCrossover;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto d = depth.getNextValue();
        const auto harmonic = harmonicMix.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            // LFO -1..1 -> 0..1, slewed so edges never click.
            const auto target = 0.5f + 0.5f * lfos[(size_t) ch].next();
            auto& s = slewed[(size_t) ch];
            s = target + slew * (s - target);

            const auto x = io[ch][i];
            const auto classicGain = 1.0f - d * (1.0f - s);
            auto y = x * classicGain;

            if (crossoverRunning)
            {
                // Lows and highs pulse in opposite directions.
                float low, high;
                crossovers[(size_t) ch].process (x, low, high);
                const auto lowGain = 1.0f - d * (1.0f - s);
                const auto highGain = 1.0f - d * s;
                const auto harmonicOut = low * lowGain + high * highGain;
                y += harmonic * (harmonicOut - y);
            }

            io[ch][i] = y;
        }

        if (numChannels == 1)
            lfos[1].next();
    }
}
