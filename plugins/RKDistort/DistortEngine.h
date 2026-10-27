#pragma once

#include "dsp/FilterCascade.h"
#include "dsp/MatchedBiquad.h"
#include "dsp/Oversampler.h"
#include "dsp/StateVariableFilter.h"
#include "dsp/Tilt.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKDistort DSP (wet signal only; the base processor does dry/wet).

        tight (low cut) -> type pre-shaping -> gain -> oversampled clipper
        -> type post-shaping -> DC blocker -> mid -> tone -> cab -> auto gain

    Types (switching crossfades curves; the pre/post filters are blended):
    - overdrive: soft clipping with the lows held back before the clipper and
      the fizz rounded after it (mid focus); smooth and touch-sensitive;
    - distortion: harder clipping, more sustain and compression;
    - fuzz: heavy asymmetric clipping with a small dead zone, so decaying
      notes sputter and gate; DC blocker after it.

    The cab filter is a fixed speaker-cabinet curve (high-pass, resonances,
    low-pass), not an impulse response; changing it crossfades. GUI-free and
    allocation-free after prepare(). */
class DistortEngine
{
public:
    static constexpr int kMaxChannels = 2;

    enum class Type { overdrive, distortion, fuzz };
    enum class Cab { off, small, medium, large, bass };

    struct Settings
    {
        Type type = Type::overdrive;
        float gainDb = 18.0f;
        float tightHz = 0.0f;        // 0 = off
        float midDb = 0.0f;          // around 800 Hz
        float tone = 0.0f;           // -1..1
        Cab cab = Cab::off;
        bool autoGain = false;
        int oversampling = 4;        // 1, 2 or 4
    };

    static float autoGainDb (float gainDb) noexcept { return -0.5f * gainDb; }

    /** The clipping curve of a type (after gain, before post-shaping). */
    static float clip (Type, float x) noexcept;

    /** A cabinet: high-pass, up to three resonance bells, low-pass. */
    class CabFilter
    {
    public:
        void configure (Cab, double sampleRate);
        void reset();
        float process (int channel, float x) noexcept;
        bool isOff() const noexcept { return off; }

        /** Response in dB (for tests and documentation). */
        double magnitudeDb (double hz, double sampleRate) const;

    private:
        bool off = true;
        rkfx::FilterCascade highPass, lowPass;
        float highPassG = 0.0f, lowPassG = 0.0f;
        std::array<rkfx::Biquad, 3> bells;
        std::array<rkfx::BiquadCoefficients, 3> bellCoefs;
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    int latencySamples() const noexcept { return rkfx::Oversampler::latencySamples (factor); }

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    struct Channel
    {
        rkfx::StateVariableFilter tight;
        float pre600 = 0.0f, pre200 = 0.0f, post = 0.0f;
        rkfx::DCBlocker dc;
        rkfx::Biquad mid;
        rkfx::Tilt tilt;
    };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Channel, kMaxChannels> channels;
    rkfx::Oversampler oversampler;
    int factor = 4;
    bool tightRunning = false;

    Type type = Type::overdrive, previousType = Type::overdrive;
    int typeFadeRemaining = 0, fadeLength = 480;

    std::array<CabFilter, 2> cabs;
    int activeCab = 0, cabFadeRemaining = 0;
    Cab cabType = Cab::off;

    float coef600 = 0.0f, coef200 = 0.0f, coefPost = 0.0f, midDbShown = 0.0f;
    rkfx::SmoothedGain gain { 8.0f }, outputGain { 1.0f };
    rkfx::Smoothed cut600, cut200, postSmooth, deadZone, midDb;
};
