#pragma once

#include "dsp/Dynamics.h"
#include "dsp/FilterCascade.h"
#include "dsp/MatchedBiquad.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKParaEQ DSP: high-pass -> low -> low mid -> mid -> high mid -> high
    -> low-pass, in series.

    - Bells and shelves use the matched design (no cramping near Nyquist).
      Their frequency, gain and Q glide, and coefficients follow every
      kSubBlock samples. A band at 0 dB (or switched off) is skipped.
    - Switching a band off glides its gain to 0 dB; switching low or high
      between shelf and bell glides to 0 dB, swaps, and glides back, so
      neither ever clicks.
    - The cut filters are true Butterworth cascades (12 to 48 dB/oct);
      switching them on/off or changing the slope crossfades over 10 ms.
    - Dynamic bands: a detector (band-pass for a bell, low-/high-pass for a
      shelf) after the high-pass sets a moving gain on top of the band's
      Gain: a negative Range cuts while the level is above the threshold, a
      positive Range boosts while it is below (only within kBoostWindowDb,
      so silence is never lifted). Their coefficients follow every
      kDynamicSubBlock samples.

    GUI-free and allocation-free after prepare(). */
class ParaEQEngine
{
public:
    static constexpr int kBands = 5;
    static constexpr int kMaxChannels = 2;
    static constexpr int kSubBlock = 32;
    static constexpr int kDynamicSubBlock = 16;
    static constexpr float kBoostWindowDb = 30.0f;   // boost acts this far below the threshold,
    static constexpr float kBoostFadeDb = 10.0f;     // then fades out over this much more
    static constexpr float kDynamicKneeDb = 6.0f;

    enum BandIndex { low, lowMid, mid, highMid, high };

    struct Dynamic
    {
        bool on = false;
        float thresholdDb = -30.0f;
        float rangeDb = -6.0f;     // < 0: cut above the threshold; > 0: boost below it
        float ratio = 3.0f;
        float attackMs = 10.0f;
        float releaseMs = 150.0f;
    };

    struct Band
    {
        bool on = true;
        bool bell = true;       // low/high: false = shelf; the mids are always bells
        float hz = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
        Dynamic dynamic;
    };

    struct Cut
    {
        bool on = false;
        float hz = 1000.0f;
        int stages = 1;         // 1..4 = 12..48 dB/oct
    };

    struct Settings
    {
        Cut highPass, lowPass;
        std::array<Band, kBands> bands;
    };

    /** The plugin's defaults: transparent (cuts off, every gain 0 dB). */
    static Settings defaults();

    /** The dynamic gain (dB) a band asks for at a detector level, before
        attack and release: 0 when the band isn't dynamic. */
    static float dynamicGainDb (const Dynamic&, float levelDb);

    /** The whole EQ's response, designed once, read at any frequency (for
        the display and tests). @p dynamicDb adds each band's current dynamic
        gain on top of its Gain (nullptr = none). */
    class Response
    {
    public:
        Response (const Settings&, double sampleRate, const std::array<float, kBands>* dynamicDb = nullptr);
        double magnitudeDb (double hz) const;

    private:
        double sampleRate;
        std::array<rkfx::BiquadCoefficients, kBands> coefficients {};
        std::array<bool, kBands> used {};
        rkfx::FilterCascade highPass, lowPass;
        bool highPassOn = false, lowPassOn = false;
        double highPassG = 0.0, lowPassG = 0.0;
    };

    /** Response of the whole EQ at @p hz in dB, for the display and tests. */
    static double magnitudeDb (const Settings&, double sampleRate, double hz);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    struct Meter
    {
        float levelDb = rkfx::Dynamics::kSilenceDb;   // loudest detector level since the last call
        float gainDb = 0.0f;                          // the dynamic gain right now
    };

    /** A band's detector level (since the last call, then reset) and current
        dynamic gain. */
    Meter takeMeter (int band) noexcept;

private:
    struct BandState
    {
        rkfx::Smoothed octave { 10.0f }, gainDb, q { 1.0f };   // octave = log2 (Hz)
        rkfx::Biquad filter;
        bool bell = true;           // shape currently running
        bool wantBell = true;       // shape asked for
        float wantGainDb = 0.0f;
        bool active = false;        // processing (gain not flat)
        float hz = 1000.0f, staticGainDb = 0.0f, currentQ = 1.0f;   // glided values in use

        // Dynamics
        Dynamic dynamic;                         // as set
        bool cutting = true;                     // direction currently running (sign of Range)
        rkfx::Smoothed dynamicScale;             // 0..1: fades the dynamic gain in and out
        std::array<rkfx::StateVariableFilter, kMaxChannels> detectorFilters;
        std::array<float, kMaxChannels> meanSquares {};   // RMS detector state (10 ms), per channel
        rkfx::Dynamics::GainSmoother smoother;   // smooths the size of the change (<= 0)
        bool detecting = false;                  // detector running
        float dynamicDb = 0.0f;                  // applied (scaled, signed)
        float designedDb = kRedesign;            // total gain the coefficients were last designed for
        Meter meter;

        bool dynamicWanted (const Band& in) const noexcept;
    };

    static constexpr float kRedesign = 1.0e9f;      // forces the next dynamic coefficient update

    double sampleRate = 48000.0;
    float rmsCoefficient = 0.0f;
    Settings settings;
    std::array<BandState, kBands> bands;

    rkfx::CrossfadingCascade highPass, lowPass;
    rkfx::Smoothed highPassOctave { 4.3f }, lowPassOctave { 14.3f };

    static rkfx::BiquadCoefficients design (int band, bool bell, double hz, double gainDb, double q, double sampleRate);
    void updateBand (int index, int numSamples);
    void updateDynamicCoefficients (int index);
    void updateDetector (int index);
    void detect (int index, const float* x, int numChannels) noexcept;
};
