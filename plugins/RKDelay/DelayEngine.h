#pragma once

#include "dsp/DelayLine.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/PitchShifter.h"
#include "dsp/RandomModulator.h"
#include "dsp/StateVariableFilter.h"
#include "dsp/TempoSync.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKDelay DSP (wet signal only; the base processor does dry/wet).

        input -> delay line (time glides like tape) -> character
        -> low cut and high cut -> feedback through DC blocker and soft
        clipper back into the delay -> ducking -> wet out

    Characters are blends of smoothed settings, so switching never clicks:
    - digital: clean, only the cut filters shape the repeats;
    - analog: each repeat darker (extra low-pass in the loop) and gently
      saturated;
    - tape: wow and flutter on the delay time, tape-style soft saturation and
      a slight loss of highs.

    Shimmer: part of the feedback goes through a pitch shifter, so each
    repeat climbs (an octave up by default). The shifted part is written into
    the line the shifter's latency early, so its repeats stay on the beat.

    Feedback at 100% sustains but never runs away (clipper and loop filters).
    The buffer holds 4 s; longer synced times are clamped. GUI-free and
    allocation-free after prepare(). */
class DelayEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr double kMaxSeconds = 4.0;

    enum class Mode { digital, analog, tape };
    enum class Stereo { mono, stereo, pingPong };

    struct Settings
    {
        Mode mode = Mode::digital;
        float timeMs = 375.0f;
        bool sync = false;
        int division = rkfx::SyncDivision::index (8, 'd');
        float feedback = 0.35f;        // 0..1
        Stereo stereo = Stereo::stereo;
        float offset = 0.0f;           // -0.5..0.5 of the left time, for the right channel
        float lowCutHz = 100.0f;
        float highCutHz = 8000.0f;
        float modDepth = 0.0f;         // 0..1
        float modRateHz = 0.6f;
        float duck = 0.0f;             // 0..1
        float shimmer = 0.0f;          // 0..1, share of the feedback that is pitch-shifted
        float shimmerRatio = 2.0f;     // pitch ratio of the shifted repeats
    };

    /** The left delay time in ms (synced or free), clamped to the buffer. */
    static double leftTimeMs (const Settings&, double bpm);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples, const rkfx::Transport&) noexcept;

private:
    struct Channel
    {
        rkfx::DelayLine line;
        rkfx::StateVariableFilter lowCut, highCut;
        rkfx::DCBlocker dc;
        rkfx::PitchShifter shifter;
        rkfx::StateVariableFilter shimmerHighPass;
        float characterLowPass = 0.0f;   // one-pole state (analog darkening, tape highs loss)
        float lastProcessed = 0.0f;
        double time = 0.0;               // smoothed delay time in samples
    };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Channel, kMaxChannels> channels;

    rkfx::RandomModulator wow { 0x9e3779b9u };
    double wowPhase = 0.0, flutterPhase = 0.0, modPhase = 0.0;
    rkfx::EnvelopeFollower inputLevel;

    float timeCoef = 0.0f;   // delay time glide
    rkfx::Smoothed feedback { 0.35f }, darkCoef, saturation, tapeAmount, modDepthSamples, duck, shimmer;
    bool shimmerRunning = false;
    rkfx::SmoothedGain lowCutHz { 100.0f }, highCutHz { 8000.0f };
};
