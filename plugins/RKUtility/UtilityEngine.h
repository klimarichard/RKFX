#pragma once

#include "dsp/Crossover.h"
#include "dsp/DelayLine.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKUtility DSP.

        channel mode and swap -> polarity -> channel delay -> width (mid/side)
        -> bass mono -> pan / balance -> gain -> mute

    Channel mode, swap and polarity form one 2x2 matrix whose coefficients
    glide over 10 ms, so switching never clicks. Everything else glides
    too; mute ramps over 10 ms. Stages at their neutral setting are skipped,
    so the defaults pass the signal bit-for-bit. The channel delay is an
    intentional effect (mic alignment) and isn't reported as latency.
    GUI-free and allocation-free after prepare(). */
class UtilityEngine
{
public:
    static constexpr float kSilentDb = -100.0f;     // gain at or below this = -inf (silence)
    static constexpr float kMaxDelayMs = 20.0f;
    static constexpr float kCmPerMs = 34.3f;        // speed of sound

    enum class ChannelMode { stereo, leftOnly, rightOnly, midSideEncode, midSideDecode };

    struct Settings
    {
        float gainDb = 0.0f;
        float pan = 0.0f;               // -1..1
        float width = 1.0f;             // 0..2
        float bassMonoHz = 0.0f;        // 0 = off
        bool invertLeft = false, invertRight = false;
        bool swap = false;
        ChannelMode channelMode = ChannelMode::stereo;
        float delayLeftMs = 0.0f, delayRightMs = 0.0f;
        bool mute = false;
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::DelayLine, 2> delays;
    std::array<rkfx::Crossover, 2> crossovers;
    bool bassMonoRunning = false;

    std::array<rkfx::Smoothed, 4> matrix;    // L' = m0 L + m1 R, R' = m2 L + m3 R
    rkfx::Smoothed delayLeft, delayRight, width { 1.0f }, gainLeft { 1.0f }, gainRight { 1.0f }, level { 1.0f },
                   muteGain { 1.0f }, bassMonoMix;

    static std::array<float, 4> matrixFor (const Settings&) noexcept;
};
