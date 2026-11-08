#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

/** RKMix's virtual soundcheck (docs/design/RKMix Recorder.md): a recording
    played back into the inputs, in place of the band.

    A recording is a folder of WAV files (RKMix's own, described by its
    session.json, or any WAVs named by their first input: "05 Keys.wav",
    "05-06 Keys.wav"). Each channel of a file feeds one input; a stereo
    file feeds two neighbouring inputs (left the odd one, right the even).

    One disk thread reads every file from one position into one ring of 32
    channels (3 s), so all the inputs stay aligned to the sample; the audio
    thread takes a block from it for every input at once. A locate (or a new
    loop, or a new mapping) fades out over 5 ms, empties the ring, refills
    it from the new place and fades back in once 0.5 s is ready. A loop
    between two markers fades over 5 ms at its seam. A ring that runs dry
    plays silence for that block and counts an underrun. Nothing on the
    audio thread allocates, locks or touches the disk. */
class MixPlayback : private juce::Thread
{
public:
    static constexpr int kInputs = 32;
    static constexpr double kRingSeconds = 3.0, kStartSeconds = 0.5, kFadeMs = 5.0;

    struct File
    {
        juce::File file;
        int numChannels = 0;
        juce::int64 length = 0;
        double sampleRate = 0.0;
        std::vector<int> inputs;          // per channel: the input it feeds (0..31), or -1
        juce::String role;                // "" for inputs, "main", "bus3": recorded outputs, never mapped by default
    };

    struct Marker
    {
        juce::int64 at = 0;
        juce::String label;
    };

    struct Recording
    {
        juce::File folder;
        double sampleRate = 0.0;
        juce::int64 length = 0;
        std::vector<File> files;
        std::vector<Marker> markers;
        juce::StringArray names;          // the recording's input names (session.json), or empty
        juce::String error;               // why it can't be loaded
    };

    /** Reads a folder: its session.json, or its WAV files. */
    static Recording scan (const juce::File& folder);

    MixPlayback();
    ~MixPlayback() override;

    // Message thread ---------------------------------------------------------
    /** Loads @p recording for a session at @p sampleRate (another rate is refused). */
    juce::Result load (const Recording& recording, double sampleRate);
    void unload();
    bool isLoaded() const noexcept { return loaded.load(); }
    const Recording& getRecording() const noexcept { return recording; }

    /** Moves file @p index to feed inputs from @p firstInput on (a stereo file:
        an even 0-based input, so its sides land on a pair); -1 unmaps it. */
    bool mapFile (int index, int firstInput);
    /** The file and channel feeding @p input, or {-1, -1}. */
    std::pair<int, int> sourceOf (int input) const;

    void play();
    void pause();
    void stop();                          // pause and back to the start
    void locate (juce::int64 sample);
    void setLoop (bool on, juce::int64 start, juce::int64 end);
    bool isLooping() const noexcept { return loopOn.load(); }
    juce::int64 getLoopStart() const noexcept { return loopStart.load(); }
    juce::int64 getLoopEnd() const noexcept { return loopEnd.load(); }

    /** The next marker after the play position (or the end); the previous one
        (half a second back, so a second press goes one further; or the start). */
    juce::int64 markerNear (bool next) const;
    /** Loops the part between the markers around the play position (or stops looping). */
    void loopAroundPosition (bool on);

    /** The loaded recording's markers, edited (message thread): kept in time
        order and saved into its session.json at once (the rest of the file is
        kept; a folder of plain WAVs gets one that holds only the markers).
        addMarker returns the new marker's index (an empty label: the next
        "Marker n"); the others say whether there was such a marker and the
        file was written. */
    int addMarker (juce::int64 at, const juce::String& label = {});
    bool renameMarker (int index, const juce::String& label);
    bool removeMarker (int index);
    /** The index of the last marker at or before @p sample, or -1. */
    int markerIndexAt (juce::int64 sample) const;

    bool isPlaying() const noexcept { return playing.load(); }
    bool isLocating() const noexcept { return requestGen.load() != readyGen.load(); }
    juce::int64 getPosition() const noexcept { return shownPosition.load(); }
    int getUnderruns() const noexcept { return underruns.load(); }

    // Audio thread -----------------------------------------------------------
    /** Allocates the block scratch. Message thread, before processing. */
    void prepare (int maxBlockSize);

    /** Writes the recording's block into the inputs set to play back (@p onPlayback),
        silence where nothing is mapped; the transport advances whether or not any
        input listens. */
    void process (float* const* inputs, const std::array<bool, kInputs>& onPlayback, int numSamples) noexcept;

    // Tests ------------------------------------------------------------------
    void stallDiskForTesting (bool s) noexcept { stalled = s; }
    /** Benchmarks: as if the disk had refilled the ring at once (stall the disk first). */
    void refillForBenchmark() noexcept { if (fifo != nullptr) fifo->finishedWrite (fifo->getFreeSpace()); }
    /** Waits (up to @p ms) until the ring is ready at the current position. */
    bool waitUntilReady (int ms = 2000) const;
    /** Tests: the audio side hasn't taken the last locate yet (process a block). */
    bool needsAudio() const noexcept { return requestGen.load() != ackGen.load(); }

private:
    Recording recording;
    std::vector<std::unique_ptr<juce::AudioFormatReader>> readers;   // by file (disk thread, under lock)
    juce::CriticalSection lock;                                      // message thread vs the disk thread
    juce::SpinLock audioLock;                                        // the audio thread (try-lock) vs load and unload

    std::unique_ptr<juce::AbstractFifo> fifo;
    juce::AudioBuffer<float> ring, chunk, scratch;
    std::array<std::pair<int, int>, kInputs> routes {};             // input -> (file, channel), the disk thread's copy

    std::atomic<bool> loaded { false }, playing { false }, stalled { false }, loopOn { false };
    std::atomic<int> requestGen { 0 }, ackGen { 0 }, readyGen { -1 }, underruns { 0 };
    std::atomic<juce::int64> seekTarget { 0 }, shownPosition { 0 }, loopStart { 0 }, loopEnd { 0 }, length { 0 };
    int fadeSamples = 240;

    // Audio thread
    int audioGen = 0;
    juce::int64 position = 0;
    float gain = 0.0f;

    // Disk thread
    int diskGen = -1;
    juce::int64 diskPosition = 0;
    bool diskLoop = false;
    juce::int64 diskLoopStart = 0, diskLoopEnd = 0;

    void run() override;
    void fill();
    void relocate (juce::int64 target);
    void updateRoutes();
    bool saveMarkers() const;
};
