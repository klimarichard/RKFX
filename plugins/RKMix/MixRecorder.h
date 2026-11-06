#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>
#include <vector>

/** RKMix's recorder (docs/design/RKMix Recorder.md): every chosen input to
    its own WAV file (a linked pair to one stereo file), the main and Aux
    buses too, for the whole gig.

    A take is a folder of files written together. The audio thread copies
    each block into one lock-free FIFO per file (4 s each, allocated when
    the take starts); a disk thread drains them. A block goes into every
    file or, when any FIFO is full (the disk can't keep up), into none: the
    files stay the same length and aligned to the sample, and the dropout is
    counted with its place. Every file's header is brought up to date every
    10 s, so a crash or a power cut loses at most the last 10 s. Past 4 GB a
    file becomes RF64 by itself (JUCE's WAV writer).

    Start, stop and splitting a take swap the take the audio thread sees
    under a spin lock held only for the swap: every file starts and stops on
    the same block, and a split loses no sample. Nothing on the audio thread
    allocates, touches the disk or waits for the disk thread. */
class MixRecorder : private juce::Thread
{
public:
    static constexpr double kFifoSeconds = 4.0;
    static constexpr double kFlushSeconds = 10.0;

    /** What one channel of a file records. */
    struct Source
    {
        enum Kind { input, main, bus };
        Kind kind = input;
        int index = 0;      // input 0..31, bus 0..15
        int side = 0;       // main and buses: 0 left, 1 right
    };

    struct Track
    {
        juce::String fileName;            // "05-06 Keys.wav"
        std::vector<Source> sources;      // one per channel of the file
    };

    struct Take
    {
        juce::File folder;
        std::vector<Track> tracks;
        double sampleRate = 48000.0;
        bool asFloat = false;             // 32-bit float, else 24-bit
        juce::var info;                   // extra for session.json (names, colours, mix name...)
    };

    MixRecorder();
    ~MixRecorder() override;

    // Message thread ---------------------------------------------------------

    /** Opens the files and starts recording on the next audio block (or, while
        recording, ends the current take and starts this one on the same block).
        Fails, recording nothing new, when a folder or file can't be made. */
    juce::Result start (const Take&);

    /** Ends the take: the audio thread stops on the next block, the files are
        finished, session.json written. */
    void stop();

    bool isRecording() const noexcept { return recording.load(); }
    juce::File getFolder() const;

    /** Samples recorded in this take (the same in every file). */
    juce::int64 getRecordedSamples() const noexcept { return recorded.load(); }
    double getSampleRate() const noexcept { return rate.load(); }
    int getDropouts() const noexcept { return dropouts.load(); }

    /** A marker at the current position ("Scene 3", "Marker 2"). */
    void addMarker (const juce::String& label);
    int getNumMarkers() const;

    /** Notes for session.json (e.g. "split: the session was restored"). */
    void addNote (const juce::String& note);

    /** Bytes the take writes per second (all files). */
    double getBytesPerSecond() const noexcept { return bytesPerSecond.load(); }

    /** Set by the disk thread when the disk is about to fill (under a minute left):
        whoever runs the recorder stops it. */
    bool isDiskNearlyFull() const noexcept { return diskNearlyFull.load(); }

    /** Bytes per second for a take of @p numChannels channels at @p sampleRate. */
    static double bytesPerSecondFor (int numChannels, double sampleRate, bool asFloat);

    // Audio thread -----------------------------------------------------------

    /** Brackets one block: every push between them belongs to it. */
    void beginBlock (int numSamples) noexcept;
    void pushInput (int input, const float* data) noexcept;
    void pushMain (const float* left, const float* right) noexcept;
    void pushBus (int bus, const float* left, const float* right) noexcept;
    void endBlock() noexcept;

    /** True while a take is running (cheap: for skipping the pushes). */
    bool isActive() const noexcept { return recording.load (std::memory_order_relaxed); }

    // Tests ------------------------------------------------------------------
    void stallDiskForTesting (bool stall) noexcept { stalled = stall; }
    /** Benchmarks: as if the disk had written everything at once (stall the disk first). */
    void discardForBenchmark() noexcept;

private:
    struct Live;
    std::unique_ptr<Live> live;              // what the audio thread writes into (swapped under swapLock)
    std::vector<std::unique_ptr<Live>> closing;
    juce::SpinLock swapLock;                 // audio thread vs the swap (held for a pointer swap)
    juce::CriticalSection diskLock;          // message thread vs the disk thread (the takes alive)

    std::atomic<bool> recording { false }, stalled { false }, diskNearlyFull { false };
    std::atomic<juce::int64> recorded { 0 };
    std::atomic<int> dropouts { 0 };
    std::atomic<double> rate { 48000.0 }, bytesPerSecond { 0.0 };

    // The block being recorded (audio thread).
    Live* blockTake = nullptr;
    int blockSamples = 0;
    bool blockOk = false;

    void run() override;
    void finish (Live&);
    void writeSession (Live&);
    static juce::int64 recordedSamplesOf (const Live&);
};
