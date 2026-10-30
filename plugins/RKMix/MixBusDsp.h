#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MixDynamics.h"
#include "MixFilters.h"
#include "safety/Smoothing.h"

#include <array>

/** The fixed processing of a bus, the main and the mono sum:

        HPF -> 6-band EQ -> LPF -> compressor   (before the inserts and fader)
        limiter                                 (after the fader)

    Every section has an On switch (the cut filters need the EQ on too);
    switching crossfades over 20 ms and an off section is skipped. The EQ
    bands are the channel EQ's matched bells and shelves, the cut filters
    Butterworth (12 or 24 dB/oct; a slope change dips the filter out and back
    in). The compressor is RKCompress's (with Auto release), the limiter
    zero-latency with a clip at the ceiling. GUI-free, allocation-free after
    prepare(). */
class BusDsp
{
public:
    static constexpr int kEqBands = 6;

    struct EqBand
    {
        bool bell = true;                  // the first and last: shelf or bell; the others are bells
        float hz = 1000.0f, gainDb = 0.0f, q = 1.0f;
    };

    struct Eq
    {
        Eq()
        {
            const float freqs[kEqBands] = { 80.0f, 160.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f };
            for (int b = 0; b < kEqBands; ++b)
                bands[(size_t) b] = { b != 0 && b != kEqBands - 1, freqs[b], 0.0f, b == 0 || b == kEqBands - 1 ? 0.7f : 1.0f };
        }
        bool on = false;
        bool hpfOn = false, hpfSteep = false, lpfOn = false, lpfSteep = false;
        float hpfHz = 40.0f, lpfHz = 16000.0f;
        std::array<EqBand, kEqBands> bands;
    };

    struct Comp
    {
        bool on = false;
        float thresholdDb = -20.0f, ratio = 2.0f, kneeDb = 6.0f;
        float attackMs = 30.0f, releaseMs = 150.0f, makeupDb = 0.0f;
        bool autoRelease = true;
    };

    struct Limit
    {
        bool on = false;
        float ceilingDb = -1.0f, releaseMs = 100.0f;
    };

    struct Settings
    {
        Eq eq;
        Comp comp;
        Limit limit;
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setSettings (const Settings&, bool snap);
    const Settings& getSettings() const noexcept { return settings; }

    /** The EQ and the compressor (before the inserts and the fader), in place. */
    void processBeforeFader (float* const* io, int numSides, int numSamples) noexcept;
    /** The limiter (after the fader), in place. */
    void processLimiter (float* const* io, int numSides, int numSamples) noexcept;

    /** True when a section is on (or fading): the strip has work to do on silence too. */
    bool isAnyOn() const noexcept;

    float takeCompReductionDb() noexcept  { return comp.takeGainReductionDb(); }
    float takeLimitReductionDb() noexcept { return limiter.takeGainReductionDb(); }

    /** The EQ's response at @p hz (for the display). */
    static double responseDb (const Eq&, double hz, double sampleRate);
    /** The compressor's static curve with makeup (for the display). */
    static float compTransferDb (const Comp&, float inputDb);

private:
    using SectionFade = MixFilters::SectionFade;
    using Biquad = MixFilters::Biquad;
    using CutFilter = MixFilters::CutFilter;

    struct BandState
    {
        rkfx::Smoothed logHz, gainDb, q;
        Biquad filter;
        bool active = false;
    };

    /** A cut filter that can be switched, glide in frequency, and change slope. */
    struct Cut
    {
        SectionFade fade;
        CutFilter filter;
        rkfx::Smoothed logHz;
        bool highPass = true, wanted = false, steep = false, changingSlope = false;
    };

    double sampleRate = 48000.0;
    Settings settings;
    juce::AudioBuffer<float> scratch;

    Cut hpf, lpf;
    SectionFade eqFade, compFade, limitFade;
    std::array<BandState, kEqBands> bands;
    MixDynamics::Compressor comp;
    MixDynamics::Limiter limiter;

    void setCut (Cut&, bool on, float hz, bool steep, bool snap);
    void processCut (Cut&, float* const* io, int numSides, int numSamples) noexcept;
    void updateBand (int band) noexcept;
    static rkfx::BiquadCoefficients designBand (int band, bool bell, double hz, double gainDb, double q, double sampleRate);
};
