#pragma once

#include "dsp/Oversampler.h"
#include "dsp/Tilt.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKSaturate DSP (wet signal only; the base processor does dry/wet).

        drive -> [transformer: low-end emphasis] -> oversampled waveshaper
        -> [de-emphasis] -> DC blocker -> [tape: high rounding] -> tone tilt
        -> auto gain

    Characters:
    - tape: soft, symmetric curve, slight high-frequency rounding;
    - tube: asymmetric curve (even harmonics), DC blocker after it;
    - transformer: tighter curve; the lows are driven harder (emphasis
      before the shaper, exact inverse after), adding weight and grit.

    Character changes crossfade the curves over 10 ms; the extra filters are
    blended amounts, so nothing clicks. Oversampling (off, 2x, 4x) adds the
    latency given by latencySamples(); changing it is meant for setup.
    Auto gain is a fixed level match for the drive (no signal analysis, so
    no added delay or pumping). GUI-free and allocation-free. */
class SaturateEngine
{
public:
    static constexpr int kMaxChannels = 2;

    enum class Character { tape, tube, transformer };

    struct Settings
    {
        float driveDb = 6.0f;
        Character character = Character::tape;
        float tone = 0.0f;           // -1..1
        bool autoGain = true;
        int oversampling = 2;        // 1, 2 or 4
    };

    /** Output gain applied by auto gain for a drive setting (dB). */
    static float autoGainDb (float driveDb) noexcept { return -0.75f * driveDb; }

    /** The static curve of a character (unity slope at 0). */
    static float shape (Character, float x) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    int getOversampling() const noexcept { return factor; }
    int latencySamples() const noexcept { return rkfx::Oversampler::latencySamples (factor); }

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    struct Channel
    {
        float emphasisLow = 0.0f;        // one-pole low band for the pre-emphasis
        float deemphasisX1 = 0.0f, deemphasisY1 = 0.0f;
        float tapeLow = 0.0f;            // one-pole state for tape high rounding
        rkfx::DCBlocker dc;
        rkfx::Tilt tilt;
    };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Channel, kMaxChannels> channels;
    rkfx::Oversampler oversampler;
    int factor = 2;

    Character character = Character::tape, previousCharacter = Character::tape;
    int fadeRemaining = 0, fadeLength = 480;

    float emphasisCoef = 0.0f, tapeCoef = 0.0f;
    rkfx::SmoothedGain drive { 2.0f }, outputGain { 1.0f };
    rkfx::Smoothed emphasis, tapeAmount;
};
