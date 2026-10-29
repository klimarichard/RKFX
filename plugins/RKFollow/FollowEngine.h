#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Dynamics.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/PitchTracker.h"

/** The RKFollow DSP: a single line (violin, voice...) to MIDI notes, pitch
    bend and an expression controller. The audio isn't touched.

    Notes follow RKVoice's tracking, made exact enough for MIDI: the pitch is
    measured over the cycles of the last kSpanMs (one cycle is a whole number
    of samples, too coarse for high notes), and counts once two spans in a
    row agree within kStablePeriod. The note is the nearest semitone; a first
    note or a jump needs kConfirmCycles cycles agreeing within kConfirmPeriod
    on it, with the tracker's own (filter) pitch agreeing too (a note's first
    cycles read off while the tracker's filter settles), and
    moves of up to kHoldRangeSemitones only change it after kHoldMs more than
    kHysteresisCents away. A note change is legato
    (new note on before the old note off); silence (kGateHysteresisDb under
    the threshold) or a lost pitch (2.5 periods without a cycle) ends it.

    MIDI is written into a juce::MidiBuffer at sample positions of the block
    (the caller passes them on); allocation-free once the buffer has room. */
class FollowEngine
{
public:
    static constexpr float kHysteresisCents = 70.0f;
    static constexpr float kHoldMs = 40.0f;
    static constexpr float kHoldRangeSemitones = 2.0f;
    static constexpr float kStablePeriod = 0.06f;
    static constexpr float kConfirmPeriod = 0.015f;    // a quarter of a semitone
    static constexpr int kConfirmCycles = 3;
    static constexpr float kSpanMs = 12.0f;
    static constexpr int kMaxSpanCycles = 15;
    static constexpr float kGateHysteresisDb = 6.0f;
    static constexpr float kVelocitySpanDb = 36.0f;    // Threshold -> Velocity Min, +36 dB -> Max
    static constexpr float kBendIntervalMs = 2.0f;     // pitch bend at most this often
    static constexpr float kExpressionIntervalMs = 5.0f;
    static constexpr int kPressure = 128;              // expression controller: channel pressure
    static constexpr int kFirstMpeChannel = 2, kLastMpeChannel = 15;

    enum class Bend { off, on, mpe };

    struct Settings
    {
        bool notes = true;
        rkfx::PitchTracker::Range range = rkfx::PitchTracker::Range::violin;
        float thresholdDb = -50.0f;
        int transpose = 0;
        int velocityMin = 40, velocityMax = 127;
        float retriggerDb = 12.0f;   // 0 = off
        Bend bend = Bend::on;
        int bendRange = 2;           // semitones
        float glideMs = 0.0f;
        int channel = 1;

        bool expression = true;
        int controller = 11;         // 0..127, or kPressure
        float floorDb = -50.0f, ceilingDb = -10.0f;
        int expressionMin = 0, expressionMax = 127;
        float curve = 0.0f;          // -1..1
        float responseMs = 50.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    /** Tracks @p numSamples of audio (summed to mono) and adds MIDI to @p midi. */
    void process (const float* const* input, int numChannels, int numSamples, juce::MidiBuffer& midi) noexcept;

    /** Ends the sounding note (note off and the bend centred), at @p sample. */
    void release (juce::MidiBuffer& midi, int sample) noexcept;

    /** For the display. */
    struct Meter
    {
        int note = -1;            // sounding MIDI note, -1 = none
        float bendCents = 0.0f;   // the pitch around it
        int velocity = 0;         // of the last note on
        float levelDb = rkfx::Dynamics::kSilenceDb;
        int expression = -1;      // last value sent, -1 = none yet
    };
    Meter takeMeter() noexcept
    {
        const auto m = meter;
        meter.levelDb = rkfx::Dynamics::kSilenceDb;
        return m;
    }

    /** The expression value (Min..Max) for a level in dB. */
    static int expressionFor (const Settings&, float levelDb);

    /** The velocity (Min..Max) for an onset level in dB. */
    static int velocityFor (const Settings&, float levelDb);

private:
    double sampleRate = 48000.0;
    Settings settings;
    bool configured = false;      // settings applied at least once
    bool mpeAnnounced = false;
    bool releasePending = false;  // a setting changed that ends the sounding note

    rkfx::PitchTracker tracker;
    rkfx::EnvelopeFollower gateLevel, slowLevel;
    rkfx::Dynamics::LevelDetector rms;

    // Tracking
    std::int64_t now = 0, lastCycle = -1, pendingSince = 0;
    std::int64_t cycleTimes[kMaxSpanCycles + 1] {};
    int cycleCount = 0;           // cycles in cycleTimes (up to kMaxSpanCycles + 1)
    float lastSpanHz = 0.0f, period = 0.0f;
    int candidateNote = -1, candidateCount = 0, sinceCycle = 0;
    bool gateOpen = false, pitchValid = false;
    float pitchMidi = 0.0f;       // the tracked pitch as a (fractional) MIDI note
    int heldNote = -1, pendingNote = -1;   // tracked note before Transpose
    float onsetPeakDb = rkfx::Dynamics::kSilenceDb;
    bool retriggerArmed = true;

    // Output
    int soundingNote = -1, soundingChannel = 1, nextMpeChannel = kFirstMpeChannel;
    float bend = 0.0f;            // smoothed deviation in semitones
    int lastBendValue = 8192, sinceBend = 0;
    float expressionLevel = 0.0f; // smoothed linear RMS
    int lastExpression = -1, sinceExpression = 0;
    Meter meter;

    int expressionChannel() const noexcept { return settings.bend == Bend::mpe ? 1 : settings.channel; }
    void announceMpe (juce::MidiBuffer&, int sample) noexcept;
    void noteOn (int trackedNote, float levelDb, juce::MidiBuffer&, int sample) noexcept;
    void noteOff (juce::MidiBuffer&, int sample) noexcept;
    void sendBend (juce::MidiBuffer&, int sample, bool force) noexcept;
    void trackSample (float x, juce::MidiBuffer&, int sample) noexcept;
};
