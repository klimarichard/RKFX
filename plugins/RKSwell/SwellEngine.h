#pragma once

#include "dsp/EnvelopeFollower.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKSwell DSP: an auto swell that fades in each note.

    - Silence: when the level falls below the threshold (with 3 dB of
      hysteresis), the swell resets; the signal is muted until it returns.
    - Swell: when sound starts, the gain rises from silence to full over the
      swell time, shaped by the curve.
    - Retrigger: new attacks during a phrase are detected level-independently
      (a fast power average jumping above a slow one, as in RKTransient). The
      retrigger amount sets how big a jump counts; 0% swells only after
      silence (like a volume pedal). On a retrigger the gain drops over about
      5 ms (never a click) and swells in again.

    GUI-free and allocation-free after prepare(). */
class SwellEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kHysteresisDb = 3.0f;
    static constexpr float kDropMs = 5.0f;

    enum class Curve { linear, exponential, sCurve };

    struct Settings
    {
        float thresholdDb = -40.0f;
        float timeMs = 400.0f;
        Curve curve = Curve::sCurve;
        float retrigger = 0.5f;     // 0..1 (0 = only after silence)
        bool link = true;
    };

    /** Gain for a swell position 0..1. */
    static float curveGain (Curve, float position) noexcept;

    /** dB jump of the fast average over the slow one that counts as a new attack
        (infinite at 0% retrigger). */
    static float onsetThresholdDb (float retrigger) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    struct Voice
    {
        rkfx::EnvelopeFollower level, onsetFast, onsetSlow;
        bool sounding = false, armed = true, dropping = false;
        float position = 0.0f;
        float gain = 0.0f;           // smoothed output gain
    };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Voice, kMaxChannels> voices;
    float riseStep = 0.0f, dropStep = 0.0f, gainSmoothing = 0.0f;

    float advance (Voice&, float detector) noexcept;
};
