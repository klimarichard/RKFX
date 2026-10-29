#pragma once

#include "dsp/DelayLine.h"
#include "dsp/PitchShifter.h"
#include "dsp/StateVariableFilter.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKVerb DSP (wet signal only; the base processor does dry/wet).

        input -> pre-delay -> early reflections (type dependent)
              -> input diffusion (4 allpasses)
              -> feedback delay network (8 lines, Householder matrix,
                 damping and decay per line, slow modulation)
              -> low cut and high cut -> width -> wet out

    Shimmer: the tail is pitch-shifted (an octave up by default), band-limited
    and fed back into the network after the diffusers, so each pass climbs
    another step. The feed is limited and soft-clipped, and muted while frozen.
    At 0% the shifter doesn't run.

    Types are blends of smoothed settings (room and hall reflection
    patterns, diffusion amount, line lengths), so switching glides:
    - room: short, dense early reflections, small space;
    - hall: sparser, longer reflections and a gradual build-up;
    - plate: no distinct reflections, dense from the start.

    Each line's loop gain comes from its length, so the decay time (RT60)
    is accurate at any size. Freeze sets the loop to unity and mutes the
    input; releasing it fades the tail with the normal decay. Mono in gives
    stereo out. GUI-free and allocation-free after prepare(). */
class ReverbEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLines = 8;
    static constexpr int kDiffusers = 4;
    static constexpr int kTaps = 8;
    static constexpr double kMaxPredelaySeconds = 1.0;

    enum class Type { room, hall, plate };

    struct Settings
    {
        Type type = Type::hall;
        float decaySeconds = 2.5f;
        float size = 0.6f;            // 0..1
        float predelayMs = 20.0f;     // already resolved from sync by the caller
        float damping = 0.5f;         // 0..1
        float lowCutHz = 150.0f;
        float highCutHz = 10000.0f;
        float modulation = 0.2f;      // 0..1
        float width = 1.0f;           // 0..1
        bool freeze = false;
        float shimmer = 0.0f;         // 0..1
        float shimmerRatio = 2.0f;    // pitch ratio of the shifted tail
    };

    /** How much of the tail goes back in at Shimmer 100%. */
    static constexpr float kShimmerFeed = 0.5f;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::DelayLine, kMaxChannels> predelay, reflections;
    std::array<std::array<rkfx::DelayLine, kDiffusers>, kMaxChannels> diffusers;
    std::array<rkfx::DelayLine, kLines> lines;
    std::array<rkfx::DCBlocker, kLines> lineDc;
    std::array<float, kLines> damped {};
    std::array<double, 4> modPhase {};
    std::array<rkfx::StateVariableFilter, kMaxChannels> lowCut, highCut;

    rkfx::Smoothed predelaySamples, sizeFactor { 0.72f }, typeScale { 1.0f }, diffusion { 0.5f },
                   roomLevel, hallLevel { 0.7f }, modSamples, width { 1.0f }, freeze;
    float decaySeconds = 2.5f, dampingCoef = 0.0f;

    rkfx::PitchShifter shimmerShifter;
    rkfx::StateVariableFilter shimmerHighPass, shimmerLowPass;
    rkfx::Smoothed shimmer;
    bool shimmerRunning = false;
};
