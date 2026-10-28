#pragma once

#include "dsp/Dynamics.h"
#include "dsp/MatchedBiquad.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKAcoustic DSP: a preamp for piezo pickups (violin, viola, acoustic
    guitar, upright bass), in series:

        high-pass -> phase -> body -> quack -> low, mid, treble, air -> notch -> compressor

    - High-pass: 24 dB/oct Butterworth. Off glides it down to 10 Hz, then
      skips it (so off is exactly transparent).
    - Body: three resonances of the instrument's body, which a piezo under
      the bridge or saddle hardly picks up, scaled by the Body amount.
    - Quack: cuts the hard, nasal peak typical of piezos, at a frequency set
      by the voicing.
    - EQ: low shelf at 100 Hz, a sweepable mid bell, treble shelf at 4 kHz,
      air shelf at 12 kHz.
    - Notch: a narrow, deep cut against feedback (Off glides it away).
    - Compressor: one amount from off to an even, gentle 4:1, soft knee,
      auto release, with a little makeup gain.

    Bells and shelves use the matched design; their frequency, gain and Q
    glide and the coefficients follow every kSubBlock samples, so voicing
    changes never click. A band at 0 dB is skipped. The phase switch ramps
    through zero over the parameter glide time. Zero latency; GUI-free and
    allocation-free after prepare(). */
class AcousticEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kSubBlock = 32;

    enum class Voicing { flat, violin, viola, guitar, bass };

    struct Settings
    {
        Voicing voicing = Voicing::guitar;
        float body = 0.5f;          // 0..1
        float quack = 0.3f;         // 0..1 (up to -12 dB)
        float highPassHz = 0.0f;    // 0 = off
        bool invert = false;
        float lowDb = 0.0f, midDb = 0.0f, midHz = 800.0f, trebleDb = 0.0f, airDb = 0.0f;
        float notchHz = 0.0f;       // 0 = off
        float compress = 0.0f;      // 0..1
    };

    /** One body resonance at 100% Body. */
    struct Resonance { float hz, gainDb, q; };
    struct Voice
    {
        std::array<Resonance, 3> body;
        float quackHz;
    };
    static const Voice& voiceFor (Voicing) noexcept;

    static constexpr float kMaxQuackCutDb = 12.0f;
    static constexpr float kNotchDepthDb = -30.0f;
    static constexpr float kNotchQ = 8.0f;

    /** Compressor threshold and ratio for an amount 0..1 (ratio 1 = off). */
    static float thresholdFor (float compress) noexcept { return -10.0f - 20.0f * compress; }
    static float ratioFor (float compress) noexcept     { return 1.0f + 3.0f * compress; }
    static float makeupFor (float compress) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    /** Response of the filters (not the compressor) at @p hz in dB, for tests. */
    static double magnitudeDb (const Settings&, double sampleRate, double hz);

private:
    enum class Shape { bell, lowShelf, highShelf };

    struct Band
    {
        Shape shape = Shape::bell;
        rkfx::Smoothed octave { 10.0f }, gainDb, q { 1.0f };   // octave = log2 (Hz)
        rkfx::Biquad filter;
        bool active = false;
    };

    enum BandIndex { body1, body2, body3, quackBand, lowBand, midBand, trebleBand, airBand, notchBand, kNumBands };

    struct Target { Shape shape; float hz, gainDb, q; };
    static std::array<Target, kNumBands> targetsFor (const Settings&);

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Band, kNumBands> bands;

    std::array<rkfx::StateVariableFilter, kMaxChannels> highPass1, highPass2;
    rkfx::Smoothed highPassOctave { 3.32f }, polarity { 1.0f }, compressAmount;

    rkfx::Dynamics::LevelDetector detector;
    rkfx::Dynamics::GainSmoother gainSmoother;

    bool highPassActive = false;
    int untilUpdate = 0;

    void updateFilters (bool snap) noexcept;
    static rkfx::BiquadCoefficients design (Shape, double hz, double gainDb, double q, double sampleRate);
};
