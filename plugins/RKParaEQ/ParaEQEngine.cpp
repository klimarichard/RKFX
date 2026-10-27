#include "ParaEQEngine.h"

#include <cmath>

namespace
{
    constexpr double kTwoPi = 6.28318530717958647692;

    bool isFlat (float gainDb) noexcept { return std::abs (gainDb) < 1.0e-6f; }
}

ParaEQEngine::Settings ParaEQEngine::defaults()
{
    Settings s;
    s.highPass = { false, 80.0f, 1 };
    s.lowPass = { false, 12000.0f, 1 };
    s.bands[low]     = { true, false, 100.0f,  0.0f, 0.71f };
    s.bands[lowMid]  = { true, true,  250.0f,  0.0f, 1.0f };
    s.bands[mid]     = { true, true,  1000.0f, 0.0f, 1.0f };
    s.bands[highMid] = { true, true,  3500.0f, 0.0f, 1.0f };
    s.bands[high]    = { true, false, 8000.0f, 0.0f, 0.71f };
    return s;
}

rkfx::BiquadCoefficients ParaEQEngine::design (int band, bool bell, double hz, double gainDb, double q, double rate)
{
    if (bell || (band != low && band != high))
        return rkfx::MatchedDesign::bell (hz, gainDb, q, rate);

    return band == low ? rkfx::MatchedDesign::lowShelf (hz, gainDb, rate)
                       : rkfx::MatchedDesign::highShelf (hz, gainDb, rate);
}

double ParaEQEngine::magnitudeDb (const Settings& s, double rate, double hz)
{
    const auto w = kTwoPi * hz / rate;
    double magnitude = 1.0;

    for (int b = 0; b < kBands; ++b)
    {
        const auto& band = s.bands[(size_t) b];
        if (band.on)
            magnitude *= design (b, band.bell, band.hz, band.gainDb, band.q, rate).magnitude (w);
    }

    rkfx::FilterCascade cascade;
    if (s.highPass.on)
    {
        cascade.configure (rkfx::FilterCascade::Mode::highpass, s.highPass.stages, false);
        magnitude *= cascade.magnitude (w, rkfx::StateVariableFilter::gFor (s.highPass.hz, rate), 0.0);
    }
    if (s.lowPass.on)
    {
        cascade.configure (rkfx::FilterCascade::Mode::lowpass, s.lowPass.stages, false);
        magnitude *= cascade.magnitude (w, rkfx::StateVariableFilter::gFor (s.lowPass.hz, rate), 0.0);
    }

    return 20.0 * std::log10 (std::max (magnitude, 1.0e-10));
}

//==============================================================================
void ParaEQEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& b : bands)
    {
        b.octave.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.gainDb.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.q.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    }

    highPassOctave.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    lowPassOctave.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    highPass.prepare (sampleRate);
    lowPass.prepare (sampleRate);

    reset();
}

void ParaEQEngine::reset()
{
    for (int i = 0; i < kBands; ++i)
    {
        auto& b = bands[(size_t) i];
        b.bell = b.wantBell;
        b.octave.setCurrentAndTargetValue (b.octave.getTargetValue());
        b.gainDb.setCurrentAndTargetValue (b.wantGainDb);
        b.q.setCurrentAndTargetValue (b.q.getTargetValue());
        b.filter.reset();
        b.active = false;
        updateBand (i, 0);
    }

    highPassOctave.setCurrentAndTargetValue (highPassOctave.getTargetValue());
    lowPassOctave.setCurrentAndTargetValue (lowPassOctave.getTargetValue());
    highPass.reset (rkfx::FilterCascade::Mode::highpass, settings.highPass.on ? settings.highPass.stages : 0, false);
    lowPass.reset (rkfx::FilterCascade::Mode::lowpass, settings.lowPass.on ? settings.lowPass.stages : 0, false);
}

void ParaEQEngine::setSettings (const Settings& s)
{
    settings = s;

    for (int i = 0; i < kBands; ++i)
    {
        const auto& in = s.bands[(size_t) i];
        auto& b = bands[(size_t) i];

        b.octave.setTargetValue (std::log2 (juce::jlimit (20.0f, 20000.0f, in.hz)));
        b.q.setTargetValue (juce::jlimit (0.1f, 20.0f, in.q));
        b.wantBell = (i == low || i == high) ? in.bell : true;
        b.wantGainDb = in.on ? juce::jlimit (-24.0f, 24.0f, in.gainDb) : 0.0f;

        // A shape change first glides to 0 dB (see updateBand).
        b.gainDb.setTargetValue (b.wantBell == b.bell ? b.wantGainDb : 0.0f);
    }

    highPassOctave.setTargetValue (std::log2 (juce::jlimit (10.0f, 20000.0f, s.highPass.hz)));
    lowPassOctave.setTargetValue (std::log2 (juce::jlimit (10.0f, 20000.0f, s.lowPass.hz)));
    highPass.setTarget (rkfx::FilterCascade::Mode::highpass, s.highPass.on ? juce::jlimit (1, 4, s.highPass.stages) : 0, false);
    lowPass.setTarget (rkfx::FilterCascade::Mode::lowpass, s.lowPass.on ? juce::jlimit (1, 4, s.lowPass.stages) : 0, false);
}

void ParaEQEngine::updateBand (int index, int numSamples)
{
    auto& b = bands[(size_t) index];

    const auto smoothing = b.octave.isSmoothing() || b.gainDb.isSmoothing() || b.q.isSmoothing();
    const auto octave = b.octave.skip (numSamples);
    const auto gain = b.gainDb.skip (numSamples);
    const auto q = b.q.skip (numSamples);

    // Flat and staying flat: swap a pending shape change, then skip the band.
    if (isFlat (gain) && ! b.gainDb.isSmoothing())
    {
        if (b.bell != b.wantBell)
        {
            b.bell = b.wantBell;
            b.gainDb.setTargetValue (b.wantGainDb);
        }

        if (isFlat (b.gainDb.getTargetValue()))
        {
            if (b.active)
            {
                b.active = false;
                b.filter.reset();
            }
            return;
        }
    }

    if (smoothing || ! b.active || numSamples == 0)
        b.filter.setCoefficients (design (index, b.bell, std::exp2 ((double) octave), gain, q, sampleRate));

    b.active = true;
}

void ParaEQEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    highPass.beginBlock();
    lowPass.beginBlock();

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const auto n = juce::jmin (kSubBlock, numSamples - start);

        for (int i = 0; i < kBands; ++i)
            updateBand (i, n);

        const auto hpOn = ! highPass.isPassThrough();
        const auto lpOn = ! lowPass.isPassThrough();
        const auto hpG = rkfx::StateVariableFilter::gFor (std::exp2 ((double) highPassOctave.skip (n)), sampleRate);
        const auto lpG = rkfx::StateVariableFilter::gFor (std::exp2 ((double) lowPassOctave.skip (n)), sampleRate);

        for (int i = start; i < start + n; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto x = io[ch][i];

                if (hpOn)
                    x = highPass.process (ch, x, hpG, 0.0f);

                for (auto& b : bands)
                    if (b.active)
                        x = b.filter.process (ch, x);

                if (lpOn)
                    x = lowPass.process (ch, x, lpG, 0.0f);

                io[ch][i] = x;
            }

            highPass.endSample();
            lowPass.endSample();
        }
    }
}
