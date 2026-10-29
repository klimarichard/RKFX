#include "ParaEQEngine.h"

#include <cmath>

using rkfx::Dynamics::compressorCurve;

namespace
{
    constexpr double kTwoPi = 6.28318530717958647692;

    bool isFlat (float gainDb) noexcept { return std::abs (gainDb) < 1.0e-6f; }
    bool hasRange (float rangeDb) noexcept { return std::abs (rangeDb) >= 0.01f; }
}

ParaEQEngine::Settings ParaEQEngine::defaults()
{
    Settings s;
    s.highPass = { false, 80.0f, 1 };
    s.lowPass = { false, 12000.0f, 1 };
    s.bands[low]     = { true, false, 100.0f,  0.0f, 0.71f, {} };
    s.bands[lowMid]  = { true, true,  250.0f,  0.0f, 1.0f, {} };
    s.bands[mid]     = { true, true,  1000.0f, 0.0f, 1.0f, {} };
    s.bands[highMid] = { true, true,  3500.0f, 0.0f, 1.0f, {} };
    s.bands[high]    = { true, false, 8000.0f, 0.0f, 0.71f, {} };
    return s;
}

float ParaEQEngine::dynamicGainDb (const Dynamic& d, float levelDb)
{
    if (! d.on || ! hasRange (d.rangeDb))
        return 0.0f;

    const auto ratio = juce::jmax (1.0f, d.ratio);

    if (d.rangeDb < 0.0f)
        return juce::jmax (d.rangeDb, compressorCurve (levelDb, d.thresholdDb, ratio, kDynamicKneeDb) - levelDb);

    // Boost: the compressor curve mirrored around the threshold (same knee),
    // only within the window below it.
    const auto under = d.thresholdDb - levelDb;
    if (under >= kBoostWindowDb + kBoostFadeDb)
        return 0.0f;

    const auto mirrored = 2.0f * d.thresholdDb - levelDb;
    auto boost = juce::jmin (d.rangeDb, mirrored - compressorCurve (mirrored, d.thresholdDb, ratio, kDynamicKneeDb));
    if (under > kBoostWindowDb)
        boost *= (kBoostWindowDb + kBoostFadeDb - under) / kBoostFadeDb;
    return boost;
}

rkfx::BiquadCoefficients ParaEQEngine::design (int band, bool bell, double hz, double gainDb, double q, double rate)
{
    if (bell || (band != low && band != high))
        return rkfx::MatchedDesign::bell (hz, gainDb, q, rate);

    return band == low ? rkfx::MatchedDesign::lowShelf (hz, gainDb, rate)
                       : rkfx::MatchedDesign::highShelf (hz, gainDb, rate);
}

//==============================================================================
ParaEQEngine::Response::Response (const Settings& s, double rate, const std::array<float, kBands>* dynamicDb)
    : sampleRate (rate)
{
    for (int b = 0; b < kBands; ++b)
    {
        const auto& band = s.bands[(size_t) b];
        used[(size_t) b] = band.on;
        if (band.on)
            coefficients[(size_t) b] = design (b, band.bell, band.hz,
                                               band.gainDb + (dynamicDb != nullptr ? (*dynamicDb)[(size_t) b] : 0.0f),
                                               band.q, rate);
    }

    highPassOn = s.highPass.on;
    lowPassOn = s.lowPass.on;
    if (highPassOn)
    {
        highPass.configure (rkfx::FilterCascade::Mode::highpass, s.highPass.stages, false);
        highPassG = rkfx::StateVariableFilter::gFor (s.highPass.hz, rate);
    }
    if (lowPassOn)
    {
        lowPass.configure (rkfx::FilterCascade::Mode::lowpass, s.lowPass.stages, false);
        lowPassG = rkfx::StateVariableFilter::gFor (s.lowPass.hz, rate);
    }
}

double ParaEQEngine::Response::magnitudeDb (double hz) const
{
    const auto w = kTwoPi * hz / sampleRate;
    double magnitude = 1.0;

    for (int b = 0; b < kBands; ++b)
        if (used[(size_t) b])
            magnitude *= coefficients[(size_t) b].magnitude (w);

    if (highPassOn)
        magnitude *= highPass.magnitude (w, highPassG, 0.0);
    if (lowPassOn)
        magnitude *= lowPass.magnitude (w, lowPassG, 0.0);

    return 20.0 * std::log10 (std::max (magnitude, 1.0e-10));
}

double ParaEQEngine::magnitudeDb (const Settings& s, double rate, double hz)
{
    return Response (s, rate).magnitudeDb (hz);
}

//==============================================================================
bool ParaEQEngine::BandState::dynamicWanted (const Band& in) const noexcept
{
    return in.on && in.dynamic.on && hasRange (in.dynamic.rangeDb);
}

void ParaEQEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    rmsCoefficient = rkfx::Dynamics::timeCoefficient (10.0f, sampleRate);

    for (auto& b : bands)
    {
        b.octave.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.gainDb.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.q.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.dynamicScale.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        for (auto& f : b.detectorFilters) f.prepare (sampleRate);
        b.smoother.prepare (sampleRate);
    }

    highPassOctave.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    lowPassOctave.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    highPass.prepare (sampleRate);
    lowPass.prepare (sampleRate);

    setSettings (settings);
    reset();
}

void ParaEQEngine::reset()
{
    for (int i = 0; i < kBands; ++i)
    {
        auto& b = bands[(size_t) i];
        b.bell = b.wantBell;
        b.cutting = b.dynamic.rangeDb < 0.0f;
        b.octave.setCurrentAndTargetValue (b.octave.getTargetValue());
        b.gainDb.setCurrentAndTargetValue (b.wantGainDb);
        b.q.setCurrentAndTargetValue (b.q.getTargetValue());

        const auto wanted = b.dynamicWanted (settings.bands[(size_t) i]);
        b.dynamicScale.setCurrentAndTargetValue (wanted ? 1.0f : 0.0f);
        b.detecting = wanted;
        b.dynamicDb = 0.0f;
        b.meter = {};
        b.smoother.reset();
        for (auto& f : b.detectorFilters) f.reset();
        b.meanSquares.fill (0.0f);
        b.designedDb = kRedesign;

        b.filter.reset();
        b.active = false;
        updateBand (i, 0);
        if (b.detecting)
            updateDetector (i);
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

        // Dynamics: a change of shape or of direction (cut <-> boost) first
        // fades the dynamic gain out, then swaps (see updateBand).
        b.dynamic = in.dynamic;
        b.dynamic.attackMs = juce::jmax (1.0f, in.dynamic.attackMs);
        b.smoother.setTimes (b.dynamic.attackMs, juce::jmax (1.0f, in.dynamic.releaseMs));

        const auto wanted = b.dynamicWanted (in);
        const auto pending = b.wantBell != b.bell || (in.dynamic.rangeDb < 0.0f) != b.cutting;
        b.dynamicScale.setTargetValue (wanted && ! pending ? 1.0f : 0.0f);

        if (wanted && ! b.detecting)
        {
            b.detecting = true;
            b.smoother.reset();
            for (auto& f : b.detectorFilters) f.reset();
            b.meanSquares.fill (0.0f);
            b.designedDb = kRedesign;
            updateDetector (i);
        }
    }

    highPassOctave.setTargetValue (std::log2 (juce::jlimit (10.0f, 20000.0f, s.highPass.hz)));
    lowPassOctave.setTargetValue (std::log2 (juce::jlimit (10.0f, 20000.0f, s.lowPass.hz)));
    highPass.setTarget (rkfx::FilterCascade::Mode::highpass, s.highPass.on ? juce::jlimit (1, 4, s.highPass.stages) : 0, false);
    lowPass.setTarget (rkfx::FilterCascade::Mode::lowpass, s.lowPass.on ? juce::jlimit (1, 4, s.lowPass.stages) : 0, false);
}

void ParaEQEngine::updateDetector (int index)
{
    auto& b = bands[(size_t) index];
    const auto q = b.bell ? juce::jlimit (0.5f, 10.0f, b.currentQ) : 0.70710678f;
    for (auto& f : b.detectorFilters)
        f.setCutoff (b.hz, q);
}

void ParaEQEngine::updateBand (int index, int numSamples)
{
    auto& b = bands[(size_t) index];
    const auto& in = settings.bands[(size_t) index];

    const auto smoothing = b.octave.isSmoothing() || b.gainDb.isSmoothing() || b.q.isSmoothing();
    const auto octave = b.octave.skip (numSamples);
    const auto gain = b.gainDb.skip (numSamples);
    const auto q = b.q.skip (numSamples);
    b.hz = (float) std::exp2 ((double) octave);
    b.staticGainDb = gain;
    b.currentQ = q;

    // The dynamic gain has faded out: swap its direction if asked, or stop detecting.
    const auto dynamicIdle = b.dynamicScale.getCurrentValue() <= 0.0f && ! b.dynamicScale.isSmoothing();
    auto stoppedDetecting = false;
    if (dynamicIdle)
    {
        b.dynamicDb = 0.0f;
        b.meter.gainDb = 0.0f;

        const auto wantCut = in.dynamic.rangeDb < 0.0f;
        if (b.cutting != wantCut)
        {
            b.cutting = wantCut;
            b.smoother.reset();
        }

        if (b.detecting && ! b.dynamicWanted (in))
        {
            b.detecting = false;
            stoppedDetecting = true;   // back to the static coefficients
        }
    }

    // Flat and staying flat: swap a pending shape change, then skip the band.
    if (isFlat (gain) && ! b.gainDb.isSmoothing() && dynamicIdle)
    {
        if (b.bell != b.wantBell)
        {
            b.bell = b.wantBell;
            b.gainDb.setTargetValue (b.wantGainDb);
            updateDetector (index);
            b.designedDb = kRedesign;
        }

        const auto pendingDirection = (in.dynamic.rangeDb < 0.0f) != b.cutting;
        b.dynamicScale.setTargetValue (b.dynamicWanted (in) && ! pendingDirection ? 1.0f : 0.0f);

        if (isFlat (b.gainDb.getTargetValue()) && b.dynamicScale.getTargetValue() <= 0.0f)
        {
            if (b.active)
            {
                b.active = false;
                b.filter.reset();
            }
            return;
        }
    }
    else if (dynamicIdle && b.bell == b.wantBell)
    {
        const auto pendingDirection = (in.dynamic.rangeDb < 0.0f) != b.cutting;
        b.dynamicScale.setTargetValue (b.dynamicWanted (in) && ! pendingDirection ? 1.0f : 0.0f);
    }

    if (b.detecting && (smoothing || ! b.active))
    {
        updateDetector (index);
        b.designedDb = kRedesign;
    }

    // Dynamic bands set their coefficients every kDynamicSubBlock samples instead.
    if (! b.detecting && (smoothing || stoppedDetecting || ! b.active || numSamples == 0))
        b.filter.setCoefficients (design (index, b.bell, b.hz, gain, q, sampleRate));

    b.active = true;
}

void ParaEQEngine::updateDynamicCoefficients (int index)
{
    auto& b = bands[(size_t) index];

    // Only when the gain has moved noticeably (a steady band costs no designs).
    const auto total = b.staticGainDb + b.dynamicDb;
    if (std::abs (total - b.designedDb) < 0.05f)
        return;

    b.designedDb = total;
    b.filter.setCoefficients (design (index, b.bell, b.hz, total, b.currentQ, sampleRate));
}

void ParaEQEngine::detect (int index, const float* x, int numChannels) noexcept
{
    auto& b = bands[(size_t) index];
    using Type = rkfx::StateVariableFilter::Type;

    // RMS over about 10 ms, stereo-linked (the louder channel), in dB straight
    // from the mean square.
    auto loudest = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& f = b.detectorFilters[(size_t) ch];
        const auto heard = b.bell ? f.processNormalisedBandpass (x[ch])
                                  : f.process (x[ch], index == low ? Type::lowpass : Type::highpass);
        auto& ms = b.meanSquares[(size_t) ch];
        ms = heard * heard + rmsCoefficient * (ms - heard * heard);
        loudest = juce::jmax (loudest, ms);
    }

    const auto levelDb = loudest > 1.0e-12f ? 10.0f * std::log10 (loudest) : rkfx::Dynamics::kSilenceDb;
    b.meter.levelDb = juce::jmax (b.meter.levelDb, levelDb);

    // Only the running direction counts while a swap of direction is pending.
    const auto wanted = dynamicGainDb (b.dynamic, levelDb);
    const auto size = b.smoother.process (b.cutting ? juce::jmin (0.0f, wanted) : -juce::jmax (0.0f, wanted));
    b.dynamicDb = b.dynamicScale.getNextValue() * (b.cutting ? size : -size);
    b.meter.gainDb = b.dynamicDb;
}

ParaEQEngine::Meter ParaEQEngine::takeMeter (int band) noexcept
{
    auto& m = bands[(size_t) band].meter;
    const auto result = m;
    m.levelDb = rkfx::Dynamics::kSilenceDb;
    return result;
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

        bool anyDetecting = false;
        for (const auto& b : bands)
            anyDetecting = anyDetecting || b.detecting;

        const auto hpOn = ! highPass.isPassThrough();
        const auto lpOn = ! lowPass.isPassThrough();
        const auto hpG = rkfx::StateVariableFilter::gFor (std::exp2 ((double) highPassOctave.skip (n)), sampleRate);
        const auto lpG = rkfx::StateVariableFilter::gFor (std::exp2 ((double) lowPassOctave.skip (n)), sampleRate);

        for (int i = start; i < start + n; ++i)
        {
            // Dynamic bands follow their moving gain every kDynamicSubBlock samples.
            if (anyDetecting && (i - start) % kDynamicSubBlock == 0)
                for (int b = 0; b < kBands; ++b)
                    if (bands[(size_t) b].detecting)
                        updateDynamicCoefficients (b);

            float x[kMaxChannels] {};
            for (int ch = 0; ch < numChannels; ++ch)
                x[ch] = hpOn ? highPass.process (ch, io[ch][i], hpG, 0.0f) : io[ch][i];

            // Every detector listens after the high-pass, before the bands.
            if (anyDetecting)
                for (int b = 0; b < kBands; ++b)
                    if (bands[(size_t) b].detecting)
                        detect (b, x, numChannels);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto y = x[ch];

                for (auto& b : bands)
                    if (b.active)
                        y = b.filter.process (ch, y);

                if (lpOn)
                    y = lowPass.process (ch, y, lpG, 0.0f);

                io[ch][i] = y;
            }

            highPass.endSample();
            lowPass.endSample();
        }
    }
}
