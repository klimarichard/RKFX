#pragma once

#include "dsp/CabFilter.h"
#include "dsp/MatchedBiquad.h"
#include "dsp/Oversampler.h"
#include "dsp/StateVariableFilter.h"
#include "safety/DCBlocker.h"
#include "safety/SoftClip.h"
#include "safety/Smoothing.h"

#include <array>
#include <cmath>

/** The RKPreamp DSP: a guitar amp's preamp, "amp in a box":

        input high-pass -> bright -> gain stages (oversampled) -> tone stack
        -> presence -> cab -> output

    - Voicings, each with its own gain staging and tone stack: Clean
      American (one stage, Fender-style stack), British Crunch (two stages,
      Marshall-style), Top Boost (two stages, chimey, Vox-inspired), Hot-Rod
      (three stages, tight, modern high gain).
    - Gain stages: tube-like asymmetric soft clipping (a biased tanh curve),
      each followed by a DC blocker and a high-cut, as between real stages.
      Every stage is level-matched at -12 dBFS, so the Gain knob changes
      the drive more than the loudness, and softer playing cleans up.
    - Tone stack: the classic passive bass / middle / treble network (the
      third-order model after D. Yeh), so the knobs interact like an amp's
      (mid scoop, treble and bass shifting each other), with its insertion
      loss made up: centred, its average level from 100 Hz to 4 kHz is 0 dB.
    - Bright: a treble boost before the gain, strongest at low gain (as a
      bright cap fades out when an amp is turned up).
    - Presence: -6 to +6 dB high shelf at 3.5 kHz after the tone stack.
    - Cab: the shared speaker-cabinet curves (core/dsp/CabFilter.h).

    Switching the voicing or the oversampling dips the output for about
    20 ms (as switching an amp's channel does), so it never clicks. Latency
    only with oversampling (4 or 5 samples). GUI-free and allocation-free
    after prepare(). */
class PreampEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxStages = 3;
    static constexpr int kSubBlock = 32;

    enum class Voicing { cleanAmerican, britishCrunch, topBoost, hotRod };

    struct Settings
    {
        Voicing voicing = Voicing::britishCrunch;
        float gain = 0.5f;                   // 0..1
        float bass = 0.5f, middle = 0.5f, treble = 0.5f;   // 0..1, like the pots
        float presence = 0.5f;               // 0..1, 0.5 = flat
        bool bright = false;
        rkfx::CabType cab = rkfx::CabType::off;
        int oversampling = 2;                // 1, 2 or 4
    };

    /** A tone stack's parts: R1 treble pot, R2 bass pot, R3 middle pot,
        R4 slope resistor (ohms); C1 treble, C2 bass, C3 middle (farads). */
    struct ToneStackParts { double r1, r2, r3, r4, c1, c2, c3; };

    struct Voice
    {
        int stages;
        float inputHighPassHz;       // coupling: how tight the lows go in
        float minGainDb, maxGainDb;  // total drive over every stage, Gain at 0 and 100%
        float bias;                  // asymmetry of the clipping
        float interstageHz;          // high-cut after each stage
        ToneStackParts stack;
    };
    static const Voice& voiceFor (Voicing) noexcept;

    /** The tone stack's analog transfer function, (b1 s + b2 s^2 + b3 s^3) /
        (1 + a1 s + a2 s^2 + a3 s^3), for pot positions 0..1. */
    struct Analog { double b1, b2, b3, a1, a2, a3; };
    static Analog toneStack (const ToneStackParts&, double treble, double middle, double bass);

    /** Digital 3rd-order coefficients (bilinear): b0..b3, a1..a3 (a0 = 1). */
    struct Digital { double b[4]; double a[4]; };
    static Digital discretise (const Analog&, double sampleRate);

    /** The tone stack's response in dB at @p hz, with the voicing's makeup gain. */
    static double toneStackDb (const Settings&, double sampleRate, double hz);

    /** One gain stage's curve (0 at 0): a biased soft clip (the suite's
        rational tanh approximation, cheap at the oversampled rate). */
    static float stageCurve (float x, float bias) noexcept { return rkfx::softClip (x + bias) - rkfx::softClip (bias); }

    /** Makeup for one stage driven by @p gain: -12 dBFS comes out at -12 dBFS. */
    static float stageMakeup (float gain, float bias) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    int latencySamples() const noexcept { return rkfx::Oversampler::latencySamples (factor); }

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    /** Third-order IIR (transposed direct form II, double state). */
    struct ThirdOrder
    {
        double s1 = 0.0, s2 = 0.0, s3 = 0.0;
        void reset() noexcept { s1 = s2 = s3 = 0.0; }
        float process (const Digital& d, float in) noexcept
        {
            const auto x = (double) in;
            const auto y = d.b[0] * x + s1;
            s1 = d.b[1] * x - d.a[1] * y + s2;
            s2 = d.b[2] * x - d.a[2] * y + s3;
            s3 = d.b[3] * x - d.a[3] * y;
            return (float) y;
        }
    };

    struct Channel
    {
        rkfx::StateVariableFilter inputHighPass;
        rkfx::Biquad bright;
        std::array<rkfx::DCBlocker, kMaxStages> dc;
        std::array<float, kMaxStages> lowPass {};
        ThirdOrder stack;
    };

    double sampleRate = 48000.0;
    Settings settings;
    Voicing voicing = Voicing::britishCrunch;
    int factor = 2;

    std::array<Channel, kMaxChannels> channels;
    rkfx::Oversampler oversampler;
    rkfx::Biquad presenceFilter;
    std::array<rkfx::CabFilter, 2> cabs;
    int activeCab = 0, cabFadeRemaining = 0, fadeLength = 480;
    rkfx::CabType cabType = rkfx::CabType::off;

    rkfx::Smoothed gain, bass, middle, treble, presence, brightDb;
    rkfx::Smoothed switchFade { 1.0f };   // dips while the voicing or oversampling changes
    Digital stackCoefs {};
    double stackMakeup = 1.0;
    float stageGain = 1.0f, makeup = 1.0f, interstageCoef = 0.0f;
    float shownBright = -1.0f, shownPresence = -1.0f;
    int untilUpdate = 0;

    void applyVoicing() noexcept;               // voicing and oversampling, from settings
    void updateStack() noexcept;
    float stageGainFor (float gainKnob) const noexcept;
};
