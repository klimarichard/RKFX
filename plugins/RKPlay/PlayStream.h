#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "PlayLimiter.h"

#include <atomic>
#include <memory>

/** One song streamed from disk: a reader on the disk thread fills a ring
    buffer, already at the device's sample rate, about 15 s ahead; the audio
    thread only copies out of it. The ring is allocated once in prepare(), so
    switching songs allocates nothing on the audio thread.

    Threads:
      - message thread: prepare(), open(), close(), while the stream is not on
        the disk thread (the owner removes it from the TimeSliceThread first)
        and no voice is reading it;
      - disk thread: useTimeSlice() reads, resamples, levels and writes the ring;
      - audio thread: read(), available(), isSourceDone(), position.

    A file at the device rate and 0 dB is copied bit-exact; any other rate
    goes through a windowed-sinc interpolator on the disk thread. The song's
    level (loudness matching) is applied there too, and a lifted song runs
    through the lookahead limiter (PlayLimiter). Mono files play on both
    sides, files with more channels play their first two. */
class PlayStream : public juce::TimeSliceClient
{
public:
    static constexpr double kBufferSeconds = 15.0;   // how far ahead the disk thread reads (the preload)
    static constexpr int kChunk = 8192;              // frames read from the file at a time

    /** How a song is played: where in the file, and how loud. */
    struct Options
    {
        double startSeconds = 0.0;
        double endSeconds = -1.0;          // < 0: the file's end
        double gainDb = 0.0;
        double limitCeilingDb = 1.0;       // <= 0: the lookahead limiter at this ceiling (a lifted song)
    };

    PlayStream() = default;
    ~PlayStream() override;

    /** Allocates the ring for @p deviceRate. Message thread, stream closed. */
    void prepare (double deviceRate);

    /** Starts streaming @p reader (owned from now on). Message thread, stream
        not on the disk thread. */
    void open (std::unique_ptr<juce::AudioFormatReader> reader, const Options&);
    void open (std::unique_ptr<juce::AudioFormatReader> r, double startSeconds) { open (std::move (r), Options { startSeconds }); }
    void close();
    bool isOpen() const noexcept { return reader != nullptr; }
    const Options& getOptions() const noexcept { return options; }

    //==============================================================================
    // Audio thread

    /** Copies up to @p numFrames into @p dest (2 channels) at @p offset;
        returns how many frames were there. */
    int read (float* const* dest, int offset, int numFrames) noexcept;

    /** Frames ready in the ring. */
    int available() const noexcept { return fifo != nullptr ? fifo->getNumReady() : 0; }

    /** The file has been read to its end (what is in the ring is all that's left). */
    bool isSourceDone() const noexcept { return sourceDone.load(); }

    /** Frames left to play at the device rate (approximate until the source is done). */
    juce::int64 framesLeft() const noexcept { return juce::jmax<juce::int64> (0, totalFrames - framesPlayed.load()); }

    //==============================================================================
    // Any thread
    double getPositionSeconds() const noexcept { return options.startSeconds + (double) framesPlayed.load() / deviceRate; }
    double getLengthSeconds() const noexcept { return lengthSeconds; }

    //==============================================================================
    // Disk thread
    int useTimeSlice() override;

    /** Fills the ring as far as it goes (tests). */
    void fillNow();

    /** Reads at least @p seconds (or to the song's end) right away, on the
        calling thread, so a song can start at once; the disk thread does the rest. */
    void prefill (double seconds);

private:
    double deviceRate = 48000.0;
    std::unique_ptr<juce::AudioFormatReader> reader;
    std::unique_ptr<juce::AbstractFifo> fifo;
    juce::AudioBuffer<float> ring;
    Options options;

    // Reading (disk thread)
    juce::int64 sourcePos = 0, sourceEnd = 0;        // next source sample to read, and where to stop
    double ratio = 1.0;                              // source samples per device frame
    juce::AudioBuffer<float> source;                 // read, not yet resampled
    int sourceCount = 0;
    juce::AudioBuffer<float> resampled;
    juce::WindowedSincInterpolator interpolators[2];
    bool sourceEnded = false;                        // the reader returned the song's last samples
    int dropFrames = 0;                              // lead-in of the interpolator and limiter, not written
    float gain = 1.0f;
    bool limiting = false, limiterFlushed = false;
    PlayLimiter limiter;

    std::atomic<bool> sourceDone { false };
    std::atomic<juce::int64> framesPlayed { 0 };
    juce::int64 totalFrames = 0;
    double lengthSeconds = 0.0;

    /** Gain and limiter on @p numFrames (in place), then into the ring (after the lead-in is dropped). */
    void deliver (float* l, float* r, int numFrames);
    void writeRing (const float* const* data, int numFrames);
    bool readSomething();
};
