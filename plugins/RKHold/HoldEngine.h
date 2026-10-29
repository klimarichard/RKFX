#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/PitchTracker.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>
#include <vector>

/** The RKHold DSP: holds a note (or any sound) as a steady drone.

    The input's last kHistorySeconds are kept; a hold captures the last
    kCaptureSeconds of them as a layer (up to kLayers):

    - Note layers (a steady pitch): two-period Hann grains cut at the pitch
      tracker's cycle marks, laid out one output period apart (PSOLA), each
      from a different mark, so the note stays alive without throbbing.
      Octave -1/+1 lays them out at twice / half the period.
    - Texture layers (chords, noise): kTextureGrainMs Hann grains from random
      places, four overlapping (a granular freeze); Octave reads them at
      half / double speed.

    Layers fade in over Attack and out over Release. The dry signal is never
    delayed. Allocation-free after prepare(). */
class HoldEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLayers = 4;
    static constexpr double kHistorySeconds = 1.0;
    static constexpr double kCaptureSeconds = 0.25;
    static constexpr float kTextureGrainMs = 80.0f;
    static constexpr float kSilenceDb = -60.0f;       // a quieter capture holds nothing
    static constexpr int kMinMarks = 3;               // cycle marks a note layer needs
    static constexpr float kStablePeriod = 0.03f;     // the last periods agree within this for a note
    static constexpr float kMaxDetuneCents = 6.0f;    // per grain, at Movement 100%

    enum class Mode { autoMode, note, texture };

    struct Settings
    {
        Mode mode = Mode::autoMode;
        rkfx::PitchTracker::Range range = rkfx::PitchTracker::Range::violin;
        bool latch = true;           // Hold Mode: Latch (else Momentary)
        bool add = false;            // Layers: Add (else Replace)
        int octave = 0;              // -1, 0, +1
        float attackMs = 80.0f, releaseMs = 600.0f;
        float movement = 0.4f;       // 0..1
        float toneHz = 12000.0f;
        float width = 0.5f;          // 0..1
        float holdGainDb = 0.0f, dryGainDb = 0.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    /** The footswitch: Latch + Replace toggles (hold / release), Latch + Add
        adds a layer per press; Momentary holds on press and releases on lift. */
    void press() noexcept;
    void lift() noexcept;

    /** Captures what's sounding as a new layer (replacing, or added).
        @returns false when there was nothing to hold (silence). */
    bool hold() noexcept;

    /** Fades every layer out (over Release). */
    void releaseAll() noexcept;

    bool isHolding() const noexcept;

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    /** For the display. */
    struct LayerInfo
    {
        bool active = false;
        int note = -1;               // the held note (MIDI), -1 for a texture
        float level = 0.0f;          // 0..1, as it fades
    };
    LayerInfo getLayer (int index) const noexcept;

private:
    struct Grain
    {
        double outStart = 0.0;       // layer clock (samples) where it starts
        double inStart = 0.0;        // position in the capture
        float length = 0.0f;         // output samples
        float rate = 1.0f;           // input samples per output sample
        bool active = false;
    };
    static constexpr int kMaxGrains = 8;

    struct Layer
    {
        bool used = false, note = false;
        std::array<std::vector<float>, kMaxChannels> capture;
        int length = 0;
        std::vector<int> marks;      // cycle marks (capture positions) with a full grain around them
        int numMarks = 0, lastMark = -1, nextSequential = 0;
        float period = 0.0f;         // input period (samples)
        int midiNote = -1;
        float gain = 0.0f, target = 0.0f;
        float pan = 0.0f;            // -1..1
        double clock = 0.0, nextGrain = 0.0;
        std::array<Grain, kMaxGrains> grains {};
        int order = 0;               // when it was added (oldest replaced first)
    };

    double sampleRate = 48000.0;
    Settings settings;

    // History
    std::array<std::vector<float>, kMaxChannels> history;
    int historyMask = 0;
    std::int64_t now = 0;
    int numChannels = 2;

    // Pitch: the tracker's cycle marks (absolute sample times).
    rkfx::PitchTracker tracker;
    std::array<std::int64_t, 1024> markTimes {};
    int markCount = 0, markHead = 0;

    std::array<Layer, kLayers + 1> layers;   // one more, for a layer fading out as a fifth is added
    int addedCount = 0;
    std::uint32_t random = 0x2545f491u;

    std::array<rkfx::StateVariableFilter, kMaxChannels> tone;
    rkfx::Smoothed holdGain { 1.0f }, dryGain { 1.0f };
    float attackStep = 0.0f, releaseStep = 0.0f;

    float nextRandom() noexcept;   // 0..1
    float outputPeriod (const Layer&) const noexcept;
    void startGrain (Layer&) noexcept;
    float readCapture (const Layer&, int channel, double position) const noexcept;
};
