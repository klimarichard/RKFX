#pragma once

#include "dsp/Dynamics.h"
#include "dsp/LinkwitzRiley4.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKMultiComp DSP: a zero-latency multiband compressor.

        input -> split at Mid -> lower half -> allpass at High -> split at Low  -> Low, Low-Mid
                              -> upper half -> allpass at Low  -> split at High -> High-Mid, High
        input -> allpass at Low, Mid and High -> dry (phase-matched)

    The four-way tree always runs: every band carries the same allpass
    phase shift, so the bands add up flat, and the dry signal gets that same
    shift so Dry/Wet never comb-filters. In 3-band mode Low-Mid and High-Mid
    are added together and compressed as one Mid band (with the Low-Mid
    settings); switching modes crossfades the two middles over 20 ms.

    Each band is RKCompress's compressor (shared Dynamics code), without the
    sidechain. GUI-free, allocation-free. */
class MultiCompEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kNumBands = 4;

    /** The four parameter sets. In 3-band mode lowMid is the Mid band and
        highMid is unused. */
    enum Band { low = 0, lowMid = 1, highMid = 2, high = 3 };

    struct BandSettings
    {
        float thresholdDb = -24.0f;
        float ratio = 2.5f;
        float attackMs = 10.0f;
        float releaseMs = 150.0f;
        float gainDb = 0.0f;
        bool solo = false;
        bool on = true;
    };

    /** The default settings of each band (slower in the lows). */
    static std::array<BandSettings, kNumBands> defaultBands()
    {
        std::array<BandSettings, kNumBands> b;
        b[low]     = { -24.0f, 3.0f, 20.0f, 200.0f };
        b[lowMid]  = { -24.0f, 2.5f, 10.0f, 150.0f };
        b[highMid] = { -24.0f, 2.5f,  5.0f, 100.0f };
        b[high]    = { -24.0f, 2.0f,  3.0f,  80.0f };
        return b;
    }

    struct Settings
    {
        bool fourBands = false;
        std::array<BandSettings, kNumBands> bands = defaultBands();
        float lowHz = 150.0f, midHz = 1000.0f, highHz = 3500.0f;
        float offsetDb = 0.0f;
        float kneeDb = 6.0f;
        rkfx::Dynamics::LevelDetector::Mode detection = rkfx::Dynamics::LevelDetector::Mode::rms;
        bool autoRelease = false;
        float link = 1.0f;     // 0..1
        bool autoGain = false;
        float mix = 1.0f;      // Dry/Wet, 0..1 (applied here, to the phase-matched dry)
    };

    struct Crossovers { float low, mid, high; };

    /** The crossover frequencies actually used: kept at least half an octave
        apart going upwards. In 3-band mode Mid is internal only (between Low
        and High), as the two middle bands are summed. */
    static Crossovers crossoversInUse (const Settings&);

    /** The bands in use, low to high (3 or 4 of Band). */
    static int bandsInUse (const Settings&, std::array<Band, kNumBands>& out);

    /** A band's threshold including Offset. */
    static float thresholdDb (const Settings&, Band);

    /** A band's gain after compression: its Gain plus, with Auto gain, half
        the reduction it would give a 0 dBFS signal. */
    static float makeupDb (const Settings&, Band);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    struct Meter
    {
        float levelDb = rkfx::Dynamics::kSilenceDb;   // loudest detector level
        float reductionDb = 0.0f;                     // most gain reduction (<= 0)
    };

    /** Each band's meter since the last call (resets it). In 3-band mode the
        Mid band reports as lowMid. */
    Meter takeMeter (Band) noexcept;

private:
    /** One band's compressor, for both channels. */
    struct Compressor
    {
        std::array<rkfx::Dynamics::LevelDetector, kMaxChannels> detectors;
        std::array<rkfx::Dynamics::GainSmoother, kMaxChannels> smoothers;
        rkfx::Smoothed threshold { -24.0f }, ratio { 2.5f }, makeup, active { 1.0f }, audible { 1.0f };
        float attackMs = 10.0f, releaseMs = 150.0f;
        bool autoRelease = false;
        Meter meter;

        void prepare (double rate);
        void reset();
        void set (const BandSettings&, float thresholdDb, float makeupDb, bool audible, bool autoRelease,
                  rkfx::Dynamics::LevelDetector::Mode);
        void copyStateFrom (const Compressor&);
        void process (float* x, int numChannels, float knee, float link) noexcept;
    };

    // Compressors: the four bands, plus the 3-band mode's Mid.
    static constexpr int kMid3 = kNumBands;
    std::array<Compressor, kNumBands + 1> compressors;

    struct Channel
    {
        rkfx::LinkwitzRiley4 splitLow, splitMid, splitHigh;
        rkfx::StateVariableFilter lowerAllpass, upperAllpass;     // High on the lower half, Low on the upper
        rkfx::StateVariableFilter dryLow, dryMid, dryHigh;
    };
    std::array<Channel, kMaxChannels> channels;

    double sampleRate = 48000.0;
    Settings settings;

    // Crossovers glide in octaves (log2 Hz); coefficients follow every 32 samples.
    rkfx::Smoothed lowOct, midOct, highOct;
    rkfx::Smoothed knee { 6.0f }, link { 1.0f }, mix { 1.0f }, fourAmount;

    void updateCrossovers() noexcept;
};
