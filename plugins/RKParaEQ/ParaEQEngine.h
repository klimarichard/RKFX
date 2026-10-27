#pragma once

#include "dsp/FilterCascade.h"
#include "dsp/MatchedBiquad.h"
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

    GUI-free and allocation-free after prepare(). */
class ParaEQEngine
{
public:
    static constexpr int kBands = 5;
    static constexpr int kMaxChannels = 2;
    static constexpr int kSubBlock = 32;

    enum BandIndex { low, lowMid, mid, highMid, high };

    struct Band
    {
        bool on = true;
        bool bell = true;       // low/high: false = shelf; the mids are always bells
        float hz = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
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

    /** Response of the whole EQ at @p hz in dB, for the display and tests. */
    static double magnitudeDb (const Settings&, double sampleRate, double hz);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    struct BandState
    {
        rkfx::Smoothed octave { 10.0f }, gainDb, q { 1.0f };   // octave = log2 (Hz)
        rkfx::Biquad filter;
        bool bell = true;           // shape currently running
        bool wantBell = true;       // shape asked for
        float wantGainDb = 0.0f;
        bool active = false;        // processing (gain not flat)
    };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<BandState, kBands> bands;

    rkfx::CrossfadingCascade highPass, lowPass;
    rkfx::Smoothed highPassOctave { 4.3f }, lowPassOctave { 14.3f };

    static rkfx::BiquadCoefficients design (int band, bool bell, double hz, double gainDb, double q, double sampleRate);
    void updateBand (int index, int numSamples);
};
