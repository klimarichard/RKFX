#pragma once

#include "dsp/MatchedBiquad.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKGraphEQ DSP: 10, 15 or 31 constant-Q bells in series at the ISO
    centre frequencies (1, 2/3 or 1/3 octave apart). Each bell is exactly one
    band spacing wide (between its half-gain points) at every gain, so
    neighbouring bands join smoothly.

    Each band count has its own gains, so switching never loses settings; only
    the active set is processed, and bands at 0 dB are skipped. Switching the
    band count crossfades from the old set to the new one over 20 ms.

    GUI-free and allocation-free after prepare(). */
class GraphEQEngine
{
public:
    enum class Mode { ten, fifteen, thirtyOne };

    static constexpr int kMaxBands = 31;
    static constexpr int kMaxChannels = 2;
    static constexpr int kSubBlock = 32;

    GraphEQEngine();

    static int bandCount (Mode);
    static const double* centres (Mode);
    /** Q that makes each bell one band spacing wide. */
    static double q (Mode);

    struct Settings
    {
        Mode mode = Mode::ten;
        std::array<std::array<float, kMaxBands>, 3> gainsDb {};   // per mode, per band
    };

    /** Response of the active band set at @p hz in dB. */
    static double magnitudeDb (const Settings&, double sampleRate, double hz);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    struct BandSet
    {
        Mode mode = Mode::ten;
        std::array<rkfx::Smoothed, kMaxBands> gain;
        std::array<rkfx::Biquad, kMaxBands> filters;
        std::array<bool, kMaxBands> active {};

        void prepare (double sampleRate);
        void reset (double sampleRate);
        void update (int numSamples, double sampleRate);
        float process (int channel, float x) noexcept;
    };

    double sampleRate = 48000.0;
    std::array<BandSet, 3> sets;
    int current = 0;
    int fadingFrom = -1, fadeRemaining = 0, fadeLength = 960;
};
