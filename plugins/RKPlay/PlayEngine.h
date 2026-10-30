#pragma once

#include "PlayStream.h"

#include <array>
#include <atomic>

/** RKPlay's audio thread: up to four song voices playing PlayStreams, with
    equal-power fades and crossfades, a global pause, and the master level.

    The message thread decides what plays (the processor) and sends commands
    through a lock-free FIFO; the engine reports back (a voice let its stream
    go, a song ran into the next, a stream ran dry) through another. Starting
    the next song at the end of the current one is done here, sample-exactly,
    so a crossfade of 0 is gapless and nothing depends on the message thread
    being on time: the processor arms the next song early.

    Every gain change is a fade: starting, stopping, pausing, crossfading. A
    fade follows a quarter sine, so two songs crossfading keep their power
    (sin² + cos² = 1). GUI-free and allocation-free after prepare(). */
class PlayEngine
{
public:
    static constexpr int kSlots = 6;                 // streams the processor owns
    static constexpr int kVoices = 4;
    static constexpr double kUnderrunSeconds = 0.25; // a stream with less left (not at its end) fades out
    static constexpr double kUnderrunFade = 0.2;

    void prepare (double sampleRate, int maxBlock);

    /** A new block size at the same rate: playback carries on. Not while processing. */
    void prepareBlock (int maxBlock) { if (scratch.getNumSamples() < maxBlock) scratch.setSize (2, maxBlock); }
    void setStream (int slot, PlayStream* stream) noexcept { streams[(size_t) slot] = stream; }

    //==============================================================================
    // Commands (message thread; acted on at the start of the next block)

    /** Starts @p slot, fading in over @p seconds, as the current song. Other voices carry on. */
    void start (int slot, double seconds, float gain);

    /** Fades every voice out over @p seconds and @p slot in over the same time; @p slot becomes current. */
    void crossfadeTo (int slot, double seconds, float gain);

    /** When the current song has @p seconds left, crossfade into @p slot over them (0: right after it, gapless). */
    void arm (int slot, double seconds, float gain);
    void disarm();

    /** Fades everything out over @p seconds; the voices then let their streams go. */
    void fadeOutAll (double seconds);

    void pause (double seconds);
    void resume (double seconds);

    void setLevel (float gain) noexcept { level.setTargetValue (gain); }

    //==============================================================================
    // Events (message thread)
    struct Event
    {
        enum class Type { released, advanced, ended, underrun };
        Type type = Type::released;
        int slot = -1;
    };
    bool popEvent (Event&);

    // State (any thread)
    int getCurrentSlot() const noexcept { return currentSlot.load(); }

    /** Commands sent so far (message thread): note it when a command names a slot. */
    juce::uint32 commandsSent() const noexcept { return sent; }

    /** True once the audio thread has handled every command up to @p sentCount
        and holds @p slot in no voice and as no armed song: its stream may be reused. */
    bool isSlotIdle (int slot, juce::uint32 sentCount) const noexcept
    {
        return ! busy[(size_t) slot].load() && handled.load() >= sentCount;
    }
    bool isPaused() const noexcept { return pausedState.load(); }

    //==============================================================================
    /** Adds the music to @p out (2 channels, cleared by the caller). Audio thread. */
    void process (float* const* out, int numSamples) noexcept;

private:
    struct Command
    {
        enum class Type { start, crossfade, arm, disarm, fadeOutAll, pause, resume };
        Type type = Type::start;
        int slot = -1;
        double seconds = 0.0;
        float gain = 1.0f;
    };

    struct Voice
    {
        bool active = false;
        int slot = -1;
        float gain = 1.0f;          // the song's own gain
        double fade = 0.0;          // 0..1, through a quarter sine
        double fadeStep = 0.0;      // per sample
        bool releasing = false;     // let the stream go when the fade reaches 0
    };

    double sampleRate = 48000.0;
    std::array<PlayStream*, kSlots> streams {};
    std::array<Voice, kVoices> voices;
    int current = -1;               // the voice playing the current song

    int armedSlot = -1;
    double armedSeconds = 0.0;
    float armedGain = 1.0f;

    double pauseFade = 1.0, pauseStep = 0.0;
    bool paused = false;
    juce::SmoothedValue<float> level { 1.0f };

    juce::AudioBuffer<float> scratch;

    juce::AbstractFifo commandFifo { 128 }, eventFifo { 128 };
    std::array<Command, 128> commands;
    std::array<Event, 128> events;
    std::atomic<int> currentSlot { -1 };
    std::atomic<bool> pausedState { false };
    std::array<std::atomic<bool>, kSlots> busy {};
    juce::uint32 sent = 0;                           // message thread
    std::atomic<juce::uint32> handled { 0 };

    void push (const Command&);
    void post (Event::Type, int slot) noexcept;
    void handle (const Command&) noexcept;
    int startVoice (int slot, double seconds, float gain) noexcept;
    void fadeOut (Voice&, double seconds) noexcept;
    void advance() noexcept;
    juce::int64 framesLeft (const Voice&) const noexcept;
    void render (float* const* out, int offset, int numSamples) noexcept;
    void release (int voice) noexcept;
    double stepFor (double seconds) const noexcept { return seconds > 0.0 ? 1.0 / (seconds * sampleRate) : 1.0; }
};
