#include "GraphEQEngine.h"

#include <cmath>

namespace
{
    constexpr double kTwoPi = 6.28318530717958647692;

    constexpr double kCentres10[] = { 31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 };

    constexpr double kCentres15[] = { 25.0, 40.0, 63.0, 100.0, 160.0, 250.0, 400.0, 630.0,
                                      1000.0, 1600.0, 2500.0, 4000.0, 6300.0, 10000.0, 16000.0 };

    constexpr double kCentres31[] = { 20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 200.0,
                                      250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0, 2000.0,
                                      2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0, 20000.0 };

    bool isFlat (float gainDb) noexcept { return std::abs (gainDb) < 1.0e-6f; }
}

GraphEQEngine::GraphEQEngine()
{
    for (int i = 0; i < 3; ++i)
        sets[(size_t) i].mode = (Mode) i;
}

int GraphEQEngine::bandCount (Mode m)
{
    return m == Mode::ten ? 10 : m == Mode::fifteen ? 15 : 31;
}

const double* GraphEQEngine::centres (Mode m)
{
    return m == Mode::ten ? kCentres10 : m == Mode::fifteen ? kCentres15 : kCentres31;
}

double GraphEQEngine::q (Mode m)
{
    const auto octaves = m == Mode::ten ? 1.0 : m == Mode::fifteen ? 2.0 / 3.0 : 1.0 / 3.0;
    const auto ratio = std::pow (2.0, octaves);
    return std::sqrt (ratio) / (ratio - 1.0);
}

double GraphEQEngine::magnitudeDb (const Settings& s, double rate, double hz)
{
    const auto w = kTwoPi * hz / rate;
    const auto& gains = s.gainsDb[(size_t) s.mode];
    double magnitude = 1.0;

    for (int b = 0; b < bandCount (s.mode); ++b)
        if (! isFlat (gains[(size_t) b]))
            magnitude *= rkfx::MatchedDesign::bell (centres (s.mode)[b], gains[(size_t) b], q (s.mode), rate).magnitude (w);

    return 20.0 * std::log10 (std::max (magnitude, 1.0e-10));
}

//==============================================================================
void GraphEQEngine::BandSet::prepare (double rate)
{
    for (auto& g : gain)
        g.reset (rate, rkfx::kParamSmoothingSeconds);
}

void GraphEQEngine::BandSet::reset (double rate)
{
    for (int b = 0; b < kMaxBands; ++b)
    {
        gain[(size_t) b].setCurrentAndTargetValue (gain[(size_t) b].getTargetValue());
        filters[(size_t) b].reset();
        active[(size_t) b] = false;
    }
    update (0, rate);
}

void GraphEQEngine::BandSet::update (int numSamples, double rate)
{
    for (int b = 0; b < bandCount (mode); ++b)
    {
        auto& g = gain[(size_t) b];
        const auto smoothing = g.isSmoothing();
        const auto value = g.skip (numSamples);

        if (isFlat (value) && ! g.isSmoothing())
        {
            if (active[(size_t) b])
            {
                active[(size_t) b] = false;
                filters[(size_t) b].reset();
            }
            continue;
        }

        if (smoothing || ! active[(size_t) b] || numSamples == 0)
            filters[(size_t) b].setCoefficients (rkfx::MatchedDesign::bell (centres (mode)[b], value, q (mode), rate));

        active[(size_t) b] = true;
    }
}

float GraphEQEngine::BandSet::process (int channel, float x) noexcept
{
    for (int b = 0; b < bandCount (mode); ++b)
        if (active[(size_t) b])
            x = filters[(size_t) b].process (channel, x);
    return x;
}

//==============================================================================
void GraphEQEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fadeLength = juce::jmax (1, (int) (0.02 * sampleRate));

    for (auto& set : sets)
        set.prepare (sampleRate);

    reset();
}

void GraphEQEngine::reset()
{
    fadingFrom = -1;
    fadeRemaining = 0;
    sets[(size_t) current].reset (sampleRate);
}

void GraphEQEngine::setSettings (const Settings& s)
{
    for (int i = 0; i < 3; ++i)
        for (int b = 0; b < kMaxBands; ++b)
            sets[(size_t) i].gain[(size_t) b].setTargetValue (juce::jlimit (-24.0f, 24.0f, s.gainsDb[(size_t) i][(size_t) b]));

    const auto wanted = (int) s.mode;
    if (wanted != current && fadeRemaining == 0)
    {
        // Start the new set from its settings and fade over to it.
        fadingFrom = current;
        current = wanted;
        sets[(size_t) current].reset (sampleRate);
        fadeRemaining = fadeLength;
    }
}

void GraphEQEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const auto n = juce::jmin (kSubBlock, numSamples - start);

        auto& now = sets[(size_t) current];
        now.update (n, sampleRate);
        if (fadeRemaining > 0)
            sets[(size_t) fadingFrom].update (n, sampleRate);

        for (int i = start; i < start + n; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto x = io[ch][i];
                auto y = now.process (ch, x);

                if (fadeRemaining > 0)
                {
                    const auto t = 1.0f - (float) fadeRemaining / (float) fadeLength;
                    const auto old = sets[(size_t) fadingFrom].process (ch, x);
                    y = old + t * (y - old);
                }

                io[ch][i] = y;
            }

            if (fadeRemaining > 0 && --fadeRemaining == 0)
                fadingFrom = -1;
        }
    }
}
