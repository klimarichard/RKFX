#pragma once

#include "dsp/Dynamics.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKDeEss DSP: a de-esser.

    Detector: the sibilance band (a high-pass at the frequency) through a
    fast envelope follower. Relative mode compares it with the full-band
    level, so it reacts the same in a quiet verse and a loud chorus;
    absolute mode compares it with a fixed threshold.

    Reduction (4:1 above the threshold, at most `range` dB, fast fixed
    timing tuned for sibilance) goes on the band above the frequency
    (split-band) or on the whole signal (wideband). The split is
    complementary (high = input - low-pass), so the bands add up to exactly
    the input: with no reduction the signal passes bit-for-bit.
    GUI-free and allocation-free after prepare(). */
class DeEssEngine
{
public:
    static constexpr int kMaxChannels = 2;

    /** Relative mode: 0 dB catches sibilance within this many dB of the full-band level. */
    static constexpr float kRelativeOffsetDb = 12.0f;
    static constexpr float kRatio = 4.0f;

    enum class Detection { relative, absolute };
    enum class Mode { split, wide };

    struct Settings
    {
        float frequencyHz = 6000.0f;
        Detection detection = Detection::relative;
        float thresholdDb = -24.0f;          // absolute
        float relativeThresholdDb = 0.0f;    // relative
        float rangeDb = 8.0f;
        Mode mode = Mode::split;
        bool listen = false;
        bool link = true;
    };

    /** Gain reduction (dB, <= 0) for a band level and a full-band level. */
    static float reductionDb (const Settings&, float bandDb, float fullDb) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    /** Most gain reduction since the last call (dB, <= 0); resets. */
    float takeGainReductionDb() noexcept;

private:
    struct Channel
    {
        rkfx::StateVariableFilter detector, split;
        rkfx::EnvelopeFollower bandLevel, fullLevel;
        rkfx::Dynamics::GainSmoother smoother;
    };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Channel, kMaxChannels> channels;
    rkfx::Smoothed listen, wide;
    float peakReduction = 0.0f;
};
