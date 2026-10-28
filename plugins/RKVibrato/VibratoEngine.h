#pragma once

#include "dsp/DelayLine.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/TempoSync.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKVibrato DSP: pure pitch vibrato (wet only: mixing dry would make it a
    chorus).

    A delay line whose delay follows the integral of the vibrato shape, so
    the pitch itself follows the shape (sine or triangle) and the deviation
    is exactly the depth (up to +/-1 semitone) at every rate: for a pitch
    ratio swing r at rate f the delay swings by r/f times the shape's
    integral. The right channel's LFO can be offset by the stereo phase.

    Rise: a level-independent onset detector (as in RKSwell, fixed
    sensitivity) restarts a fade-in of the depth on each new note. The delay
    line adds a few milliseconds of offset while depth is up, as in any
    vibrato pedal; the plugin still reports zero latency.
    GUI-free and allocation-free after prepare(). */
class VibratoEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kMaxSemitones = 1.0f;
    static constexpr float kMinDelayMs = 0.5f;
    static constexpr float kOnsetJumpDb = 6.0f;

    enum class Shape { sine, triangle };

    struct Settings
    {
        float rateHz = 5.0f;
        float depth = 0.3f;          // 0..1 of a semitone
        Shape shape = Shape::sine;
        bool sync = false;
        int division = rkfx::SyncDivision::index (16);
        float phaseDeg = 0.0f;
        float riseSeconds = 0.0f;
    };

    /** The shape's integral over one cycle, minus its middle: the delay swing
        (in units of r/f) at phase 0..1. Range: +/- half the swing. */
    static double shapeIntegral (Shape, double phase) noexcept;
    /** Peak-to-peak of shapeIntegral (sine: 1/pi, triangle: 1/4). */
    static double integralSwing (Shape) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples, const rkfx::Transport&) noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::DelayLine, kMaxChannels> lines;
    double phase = 0.0;
    rkfx::EnvelopeFollower onsetFast, onsetSlow;
    bool armed = true;
    float rise = 1.0f, riseStep = 1.0f;

    rkfx::Smoothed ratioSwing, rate { 5.0f };
};
