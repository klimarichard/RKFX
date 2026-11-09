#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "MixEngine.h"
#include "MixInserts.h"
#include "MixPlayback.h"

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

/** RKMix's export: the main mix of a region of the loaded recording (between
    two markers) into a stereo WAV, faster than real time.

    A copy of the mix as it is when Export is pressed (its settings, the
    faders and mutes; solo, talkback and the oscillator left out) and a copy
    of every insert run on a background thread, so the console carries on.
    Every file of the recording feeds its mapped input, whatever the input's
    Live / Playback switch says. The mix starts 2 s before the region (those
    seconds are thrown away, so compressors and gates have settled and a
    reverb already rings); the inserts' latency is taken out, so the file
    starts on the marker's sample. After the region the inputs go silent and
    the tail rings for up to 3 s, cut where it falls under -70 dB and faded
    over 20 ms. The main is taken after its limiter (where the recorder takes
    it). A cancelled or failed file leaves nothing behind.

    Scene markers ("Scene N"): the mix can change on the way, as the console
    did at the gig. Each change is a few steps (a recall's glide, sampled),
    worked out on the message thread from the scenes (MixScenes::replay); the
    run starts in the state of the last change before it, and applies the
    later ones at their samples, up to the region's end (the tail rings in
    the region's last scene). */
class MixExport : private juce::Thread
{
public:
    static constexpr double kPreRollSeconds = 2.0, kMaxTailSeconds = 3.0, kTailFloorDb = -70.0, kTailFadeMs = 20.0;
    static constexpr int kBlock = 512;

    /** The mix at one moment: the engine's settings and levels, the inserts' on/off and scene-controlled knobs. */
    struct State
    {
        MixEngine::Settings settings;
        MixEngine::Levels levels;
        std::array<bool, InsertRack::kTotalSlots> insertOn {};
        std::vector<float> knobs;                // normalised, by Mix::knobs
    };

    /** A scene marker's recall: steps[k] from sample at + k * stepSamples of the recording. */
    struct Change
    {
        juce::int64 at = 0;
        int stepSamples = 0;
        std::vector<State> steps;
    };

    struct KnobRef
    {
        int slot = 0;                            // InsertRack flat index
        juce::String paramId;
    };

    /** The mix to export through (made on the message thread). */
    struct Mix
    {
        MixEngine::Settings settings;            // as it is now (until the first change)
        MixEngine::Levels levels;
        std::array<bool, InsertRack::kTotalSlots> insertOn {};
        std::vector<float> knobs;
        std::vector<KnobRef> knobRefs;           // the scene-controlled insert knobs
        std::vector<Change> changes;             // in time order
        std::unique_ptr<InsertRack> inserts;     // a copy of the console's, or nullptr
        double bpm = 120.0;                      // for tempo-synced inserts
    };

    struct Region
    {
        juce::int64 start = 0, end = 0;          // samples of the recording
        juce::String name;                       // the file's name
    };

    MixExport();
    ~MixExport() override;

    // Message thread ---------------------------------------------------------
    /** Exports each region into its own file in @p folder (one after another),
        in the background. False if an export is running already. */
    bool start (Mix, const MixPlayback::Recording&, std::vector<Region>, const juce::File& folder, bool asFloat);
    void cancel();
    bool isRunning() const noexcept { return running.load(); }
    /** 0..1 over every region. */
    float getProgress() const noexcept { return progress.load(); }
    /** While running: "Take 03 (2 of 5)"; after: "Exported 5 files", "Cancelled", or why it failed. */
    juce::String getStatus() const;
    bool lastFailed() const;
    /** Bumped when an export ends. */
    int getVersion() const noexcept { return version.load(); }

    /** The file a region's name goes to in @p folder (a legal name; never over an existing file). */
    static juce::File fileFor (const juce::File& folder, const juce::String& name);

    /** One region into @p file, on the calling thread (the tests use it).
        @p onProgress (0..1) returns false to stop. */
    static juce::Result render (Mix&, const MixPlayback::Recording&, const Region&, const juce::File& file, bool asFloat,
                                const std::function<bool (float)>& onProgress = {});

private:
    Mix mix;
    MixPlayback::Recording recording;
    std::vector<Region> regions;
    juce::File folder;
    bool asFloat = false;

    std::atomic<bool> running { false };
    std::atomic<float> progress { 0.0f };
    std::atomic<int> version { 0 };
    mutable juce::CriticalSection statusLock;
    juce::String status;
    bool failed = false;

    void setStatus (const juce::String&, bool failure = false);
    void run() override;
};
