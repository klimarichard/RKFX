#include "MultiCompEngine.h"

#include <cmath>

using rkfx::Dynamics::compressorCurve;
using rkfx::Dynamics::toDb;

namespace
{
    constexpr float kHalfOctave = 1.41421356f;
    constexpr int kCoefficientInterval = 32;

    /** dB to gain with exp() (cheaper than the pow() in Dynamics::toGain). */
    inline float fastGain (float db) noexcept
    {
        return std::exp (db * 0.11512925f);   // ln(10) / 20
    }

    /** Equal-power Dry/Wet, exact at the ends (as the base processor's). */
    void mixGains (float m, float& dryGain, float& wetGain) noexcept
    {
        if (m >= 1.0f) { dryGain = 0.0f; wetGain = 1.0f; return; }
        if (m <= 0.0f) { dryGain = 1.0f; wetGain = 0.0f; return; }

        const auto angle = m * juce::MathConstants<float>::halfPi;
        dryGain = std::cos (angle);
        wetGain = std::sin (angle);
    }
}

MultiCompEngine::Crossovers MultiCompEngine::crossoversInUse (const Settings& s)
{
    Crossovers c;
    c.low = juce::jlimit (20.0f, 20000.0f, s.lowHz);

    if (s.fourBands)
    {
        c.mid = juce::jmax (s.midHz, c.low * kHalfOctave);
        c.high = juce::jmax (s.highHz, c.mid * kHalfOctave);
    }
    else
    {
        // The two middle bands are summed: Mid only has to sit between Low and High.
        c.high = juce::jmax (s.highHz, c.low * kHalfOctave);
        c.mid = c.high >= 2.0f * c.low ? juce::jlimit (c.low * kHalfOctave, c.high / kHalfOctave, s.midHz)
                                       : std::sqrt (c.low * c.high);
    }

    return c;
}

int MultiCompEngine::bandsInUse (const Settings& s, std::array<Band, kNumBands>& out)
{
    if (s.fourBands)
    {
        out = { low, lowMid, highMid, high };
        return 4;
    }

    out = { low, lowMid, high, high };
    return 3;
}

float MultiCompEngine::thresholdDb (const Settings& s, Band b)
{
    return s.bands[(size_t) b].thresholdDb + s.offsetDb;
}

float MultiCompEngine::makeupDb (const Settings& s, Band b)
{
    const auto& band = s.bands[(size_t) b];
    if (! s.autoGain)
        return band.gainDb;

    return band.gainDb - 0.5f * compressorCurve (0.0f, thresholdDb (s, b), band.ratio, s.kneeDb);
}

//==============================================================================
void MultiCompEngine::Compressor::prepare (double rate)
{
    for (auto& d : detectors) d.prepare (rate);
    for (auto& s : smoothers) s.prepare (rate);
    for (auto* s : { &threshold, &ratio, &makeup, &active, &audible })
        s->reset (rate, rkfx::kParamSmoothingSeconds);
}

void MultiCompEngine::Compressor::reset()
{
    for (auto& d : detectors) d.reset();
    for (auto& s : smoothers) s.reset();
    for (auto* s : { &threshold, &ratio, &makeup, &active, &audible })
        s->setCurrentAndTargetValue (s->getTargetValue());
    meter = {};
}

void MultiCompEngine::Compressor::set (const BandSettings& b, float thresholdDb, float makeupDb, bool isAudible,
                                       bool shouldAutoRelease, rkfx::Dynamics::LevelDetector::Mode mode)
{
    // Back on after being fully off: start from no reduction, not the old state.
    if (b.on && active.getCurrentValue() <= 0.0f)
        for (auto& s : smoothers)
            s.reset();

    threshold.setTargetValue (thresholdDb);
    ratio.setTargetValue (juce::jlimit (1.0f, 100.0f, b.ratio));
    makeup.setTargetValue (makeupDb);
    active.setTargetValue (b.on ? 1.0f : 0.0f);
    audible.setTargetValue (isAudible ? 1.0f : 0.0f);

    attackMs = b.attackMs;
    releaseMs = b.releaseMs;
    autoRelease = shouldAutoRelease;

    for (auto& d : detectors)
        d.setMode (mode);

    for (auto& s : smoothers)
    {
        s.setTimes (attackMs, releaseMs);
        s.setAutoRelease (autoRelease);
    }
}

void MultiCompEngine::Compressor::copyStateFrom (const Compressor& other)
{
    detectors = other.detectors;
    smoothers = other.smoothers;

    // The copy carries the other band's times: put this band's back.
    for (auto& s : smoothers)
    {
        s.setTimes (attackMs, releaseMs);
        s.setAutoRelease (autoRelease);
    }
}

void MultiCompEngine::Compressor::process (float* x, int numChannels, float kneeDb, float linkAmount) noexcept
{
    const auto t = threshold.getNextValue();
    const auto r = ratio.getNextValue();
    const auto m = makeup.getNextValue();
    const auto a = active.getNextValue();
    const auto v = audible.getNextValue();

    float level[kMaxChannels] {};
    auto loudestLevel = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        level[ch] = detectors[(size_t) ch].process (x[ch]);
        loudestLevel = juce::jmax (loudestLevel, level[ch]);
    }

    const auto loudest = toDb (loudestLevel);
    meter.levelDb = juce::jmax (meter.levelDb, loudest);

    // A band that is off passes unchanged (still metered, still in the sum).
    if (a <= 0.0f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            x[ch] *= v;
        return;
    }

    auto gainFor = [&] (size_t ch, float inDb)
    {
        const auto reduction = a * smoothers[ch].process (compressorCurve (inDb, t, r, kneeDb) - inDb);
        meter.reductionDb = juce::jmin (meter.reductionDb, reduction);

        const auto db = reduction + a * m;
        return (db == 0.0f ? 1.0f : fastGain (db)) * v;
    };

    // Fully linked: both channels get the same gain, computed once (the
    // second smoother follows, so unlinking later starts from the same state).
    if (linkAmount >= 1.0f)
    {
        const auto g = gainFor (0, loudest);
        for (size_t ch = 1; ch < (size_t) numChannels; ++ch)
            smoothers[ch] = smoothers[0];
        for (int ch = 0; ch < numChannels; ++ch)
            x[ch] *= g;
        return;
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto inDb = linkAmount * loudest + (1.0f - linkAmount) * toDb (level[ch]);
        x[ch] *= gainFor ((size_t) ch, inDb);
    }
}

//==============================================================================
void MultiCompEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& c : compressors)
        c.prepare (sampleRate);

    for (auto& c : channels)
    {
        for (auto* split : { &c.splitLow, &c.splitMid, &c.splitHigh })
            split->prepare (sampleRate);
        for (auto* f : { &c.lowerAllpass, &c.upperAllpass, &c.dryLow, &c.dryMid, &c.dryHigh })
            f->prepare (sampleRate);
    }

    for (auto* s : { &lowOct, &midOct, &highOct, &knee, &link, &mix, &fourAmount })
        s->reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void MultiCompEngine::reset()
{
    for (auto* s : { &lowOct, &midOct, &highOct, &knee, &link, &mix, &fourAmount })
        s->setCurrentAndTargetValue (s->getTargetValue());

    for (auto& c : compressors)
        c.reset();

    updateCrossovers();

    for (auto& c : channels)
    {
        for (auto* split : { &c.splitLow, &c.splitMid, &c.splitHigh })
            split->reset();
        for (auto* f : { &c.lowerAllpass, &c.upperAllpass, &c.dryLow, &c.dryMid, &c.dryHigh })
            f->reset();
    }
}

void MultiCompEngine::setSettings (const Settings& s)
{
    // Switching modes: the middle that fades in starts from the state of the one fading out.
    if (s.fourBands && fourAmount.getCurrentValue() <= 0.0f)
    {
        compressors[lowMid].copyStateFrom (compressors[kMid3]);
        compressors[highMid].copyStateFrom (compressors[kMid3]);
    }
    else if (! s.fourBands && fourAmount.getCurrentValue() >= 1.0f)
    {
        compressors[kMid3].copyStateFrom (compressors[lowMid]);
    }

    settings = s;

    const auto x = crossoversInUse (s);
    lowOct.setTargetValue (std::log2 (x.low));
    midOct.setTargetValue (std::log2 (x.mid));
    highOct.setTargetValue (std::log2 (x.high));

    knee.setTargetValue (juce::jmax (0.0f, s.kneeDb));
    link.setTargetValue (juce::jlimit (0.0f, 1.0f, s.link));
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, s.mix));
    fourAmount.setTargetValue (s.fourBands ? 1.0f : 0.0f);

    const auto& b = s.bands;
    const auto anySolo3 = b[low].solo || b[lowMid].solo || b[high].solo;
    const auto anySolo4 = anySolo3 || b[highMid].solo;
    const auto anySolo = s.fourBands ? anySolo4 : anySolo3;

    auto setBand = [&] (int index, Band band, bool soloing)
    {
        compressors[(size_t) index].set (b[(size_t) band], thresholdDb (s, band), makeupDb (s, band),
                                         ! soloing || b[(size_t) band].solo, s.autoRelease, s.detection);
    };

    setBand (low, low, anySolo);
    setBand (lowMid, lowMid, anySolo4);
    setBand (highMid, highMid, anySolo4);
    setBand (high, high, anySolo);
    setBand (kMid3, lowMid, anySolo3);
}

void MultiCompEngine::updateCrossovers() noexcept
{
    const auto lo = std::exp2 (lowOct.getCurrentValue());
    const auto mid = std::exp2 (midOct.getCurrentValue());
    const auto hi = std::exp2 (highOct.getCurrentValue());

    for (auto& c : channels)
    {
        c.splitLow.setFrequency (lo);
        c.splitMid.setFrequency (mid);
        c.splitHigh.setFrequency (hi);
        c.lowerAllpass.setCutoff (hi);
        c.upperAllpass.setCutoff (lo);
        c.dryLow.setCutoff (lo);
        c.dryMid.setCutoff (mid);
        c.dryHigh.setCutoff (hi);
    }
}

MultiCompEngine::Meter MultiCompEngine::takeMeter (Band band) noexcept
{
    auto& c = compressors[(size_t) (band == lowMid && ! settings.fourBands ? kMid3 : band)];
    const auto m = c.meter;
    c.meter = {};
    return m;
}

void MultiCompEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    for (int start = 0; start < numSamples; start += kCoefficientInterval)
    {
        const auto n = juce::jmin (kCoefficientInterval, numSamples - start);

        if (lowOct.isSmoothing() || midOct.isSmoothing() || highOct.isSmoothing())
        {
            lowOct.skip (n);
            midOct.skip (n);
            highOct.skip (n);
            updateCrossovers();
        }

        float dryGain, wetGain;
        mixGains (mix.getTargetValue(), dryGain, wetGain);

        for (int i = start; i < start + n; ++i)
        {
            const auto k = knee.getNextValue();
            const auto l = link.getNextValue();
            const auto f = fourAmount.getNextValue();

            if (mix.isSmoothing())
                mixGains (mix.getNextValue(), dryGain, wetGain);

            float bands[kNumBands][kMaxChannels] {}, dry[kMaxChannels] {};

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& c = channels[(size_t) ch];
                const auto x = io[ch][i];

                float lower, upper;
                c.splitMid.process (x, lower, upper);
                c.splitLow.process (c.lowerAllpass.processAllpass (lower), bands[low][ch], bands[lowMid][ch]);
                c.splitHigh.process (c.upperAllpass.processAllpass (upper), bands[highMid][ch], bands[high][ch]);

                dry[ch] = c.dryHigh.processAllpass (c.dryMid.processAllpass (c.dryLow.processAllpass (x)));
            }

            compressors[low].process (bands[low], numChannels, k, l);
            compressors[high].process (bands[high], numChannels, k, l);

            float middle3[kMaxChannels] {};
            if (f < 1.0f)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                    middle3[ch] = bands[lowMid][ch] + bands[highMid][ch];
                compressors[kMid3].process (middle3, numChannels, k, l);
            }

            if (f > 0.0f)
            {
                compressors[lowMid].process (bands[lowMid], numChannels, k, l);
                compressors[highMid].process (bands[highMid], numChannels, k, l);
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto middle = (1.0f - f) * middle3[ch] + f * (bands[lowMid][ch] + bands[highMid][ch]);
                const auto wet = bands[low][ch] + middle + bands[high][ch];
                io[ch][i] = dry[ch] * dryGain + wet * wetGain;
            }
        }
    }
}
