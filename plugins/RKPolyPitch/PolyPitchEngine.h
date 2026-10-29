#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/AlignedPitchShifter.h"
#include "dsp/PitchShifter.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKPolyPitch DSP: a polyphonic pitch shifter (no tracking), whammy
    style: the main voice uses the AlignedPitchShifter (exact pitch with
    short windows), the Detune voices the shimmer PitchShifter (their ratios
    are near 1, where its splices don't matter).

    - Whammy: only the shifted sound (it trails the input by the shifter's
      delay: the one RKFX latency exception).
    - Harmony: the dry signal (untouched) plus the shifted voice.
    - Detune: the dry signal plus two voices +-Detune cents (short windows).

    The shift is Heel + (Toe - Heel) x the pedal position (glided), moved to
    Toe by Momentary over Rise and back over Fall, plus Fine. The main
    shifter's window and search span follow the Range (the search covers half
    a period of its lowest note), the window shortened by a third with Low
    Latency; a window change dips the shifted
    voice for kWindowFadeMs. Mode changes crossfade the mix gains.
    Allocation-free after prepare(). */
class PolyPitchEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kDetuneWindowMs = 10.0f;
    static constexpr float kSecondWindow = 0.7f;      // the second tap pair's window, relative
    static constexpr float kLowLatencyFactor = 2.0f / 3.0f;
    static constexpr float kWindowFadeMs = 10.0f;

    enum class Mode { whammy, harmony, detune };
    enum class Range { violin, guitar, bass };

    struct Settings
    {
        Mode mode = Mode::whammy;
        Range range = Range::guitar;
        float heel = 0.0f, toe = 12.0f;   // semitones
        float pedal = 0.0f;               // 0..1
        float glideMs = 30.0f;
        bool momentary = false;
        float riseMs = 50.0f, fallMs = 150.0f;
        float fineCents = 0.0f;
        float detuneCents = 10.0f;
        float voiceGainDb = -3.0f;
        float width = 0.6f;
        float toneHz = 20000.0f;
        bool lowLatency = false;
    };

    /** The main window and alignment search (ms) for a range. */
    static float windowMs (Range, bool lowLatency);
    static float searchMs (Range);

    /** The delay of the shifted sound in Whammy and Harmony (ms). */
    static float delayMs (Range, bool lowLatency);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    /** The shift right now (semitones) and the pedal position (0..1), for the display. */
    float getShift() const noexcept { return shownShift; }
    float getPosition() const noexcept { return shownPosition; }

private:
    double sampleRate = 48000.0;
    Settings settings;

    rkfx::AlignedPitchShifter main;
    std::array<rkfx::PitchShifter, kMaxChannels> up, down;
    std::array<rkfx::StateVariableFilter, kMaxChannels> toneMain, toneUp, toneDown;

    rkfx::Smoothed dryGain { 0.0f }, mainGain { 1.0f }, detuneGain;
    float pedal = 0.0f, momentaryPosition = 0.0f;
    float windowFade = 1.0f;                    // dips to 0 around a window change
    float appliedWindowMs = 0.0f, pendingWindowMs = 0.0f;
    Range appliedRange = Range::guitar;
    float shownShift = 0.0f, shownPosition = 0.0f;
};
