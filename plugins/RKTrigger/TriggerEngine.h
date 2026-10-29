#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Dynamics.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKTrigger DSP: hits (piezo, drum mic, a knock) to MIDI notes, one
    trigger (the inputs summed) or two (left and right).

    Per trigger: key filter (high-pass, low-pass) -> |x| -> a crossing of the
    threshold starts a scan; the scan's highest peak sets the velocity and
    the note goes out at its end. Then the mask: no new hit, unless one is
    Retrigger dB above the level so far. After the mask a new hit must still
    rise kRearmDb above it, so a drum's long ring never fires again. That
    level follows |x| with a kReferenceAttackMs attack (a new hit outruns it,
    a ring doesn't) and a kMaskEnvelopeMs release. In Dual mode a hit
    within kCrosstalkMs of a stronger one on the other trigger is dropped
    when below Crosstalk of it. Each note ends after Length.

    The audio passes through (Thru) or is faded out (Mute). MIDI is written
    into a juce::MidiBuffer at sample positions of the block. */
class TriggerEngine
{
public:
    static constexpr int kTriggers = 2;
    static constexpr float kCrosstalkMs = 5.0f;
    static constexpr float kMaskEnvelopeMs = 50.0f;   // the reference level's release
    static constexpr float kReferenceAttackMs = 5.0f; // and its attack
    static constexpr float kRearmDb = 6.0f;           // after the mask, a new hit rises this far above it

    enum class Mode { mono, dual };

    struct Trigger
    {
        float thresholdDb = -30.0f;
        float highPassHz = 0.0f;     // 0 = off
        float lowPassHz = 0.0f;      // 0 = off
        float scanMs = 2.0f;
        float maskMs = 40.0f;
        float retriggerDb = 6.0f;    // 0 = off
        float dynamicsDb = 30.0f;
        float curve = 0.0f;          // -1..1
        int velocityMin = 1, velocityMax = 127;
        int note = 36;
        int channel = 10;
        float lengthMs = 50.0f;
    };

    struct Settings
    {
        Mode mode = Mode::mono;
        float crosstalk = 0.5f;      // 0..1
        bool mute = false;
        std::array<Trigger, kTriggers> triggers { Trigger {}, Trigger { -30.0f, 0.0f, 0.0f, 2.0f, 40.0f, 6.0f, 30.0f, 0.0f, 1, 127, 38, 10, 50.0f } };
    };

    /** The velocity for a hit's peak in dB. */
    static int velocityFor (const Trigger&, float peakDb);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    /** Detects hits in @p io (and mutes it if set), adding MIDI to @p midi. */
    void process (float* const* io, int numChannels, int numSamples, juce::MidiBuffer& midi) noexcept;

    /** Ends every sounding note at @p sample. */
    void release (juce::MidiBuffer& midi, int sample) noexcept;

    /** For the display: per trigger, the loudest filtered level since the
        last call, and the hits counted so far with the last one's velocity. */
    struct Meter
    {
        float levelDb = rkfx::Dynamics::kSilenceDb;
        int hits = 0;
        int velocity = 0;
    };
    Meter takeMeter (int trigger) noexcept;

private:
    struct State
    {
        rkfx::StateVariableFilter highPass, lowPass;
        int scanLeft = 0;            // > 0: scanning
        float scanPeak = 0.0f;
        int maskLeft = 0;            // > 0: masked
        float envelope = 0.0f;       // the reference level (slow attack, see above)
        int sinceHit = 1 << 20;      // samples since the last emitted hit
        float hitPeak = 0.0f;        // its peak
        bool suppressed = false;     // this scan was dropped as crosstalk
        int soundingNote = -1, soundingChannel = 10, noteLeft = 0;
        Meter meter;
    };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<State, kTriggers> states;
    rkfx::Smoothed audioGain { 1.0f };
    bool releasePending = false;   // the mode, a note or a channel changed: end sounding notes first
    float envelopeCoef = 0.0f, attackCoef = 0.0f;

    void emit (int index, juce::MidiBuffer&, int sample) noexcept;
    void detect (int index, float x, juce::MidiBuffer&, int sample) noexcept;
};
