#pragma once

#include "PlayStream.h"

#include "dsp/PeakLimiter.h"

#include <array>
#include <atomic>

/** RKPlay's audio thread: up to four song voices playing PlayStreams, with
    equal-power fades and crossfades, a global pause, ducking, eight one-shot
    pads, a pre-listen voice on the cue output, and the master level.

        songs (fades, crossfades, pause) -> duck -> + pads -> level -> safety limiter -> out
        pre-listen -> its level -> cue output (untouched by the rest)

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
    static constexpr int kPads = 8;
    static constexpr double kPadFadeIn = 0.002, kPadFadeOut = 0.03, kCueFade = 0.03;

    /** A pad's sound: the whole file at the device rate, its level applied. */
    struct PadSound
    {
        juce::AudioBuffer<float> audio;   // 2 channels
    };

    /** Ducking: the music dips while the key (the input) is above the
        threshold, while Duck is on, or while a pad that ducks plays. */
    struct DuckSettings
    {
        bool automatic = false;           // from the key input
        float depthDb = -12.0f, thresholdDb = -35.0f;
        float attackSeconds = 0.2f, holdSeconds = 1.0f, releaseSeconds = 1.5f;
    };

    void prepare (double sampleRate, int maxBlock);

    /** A new block size at the same rate: playback carries on. Not while processing. */
    void prepareBlock (int maxBlock)
    {
        if (scratch.getNumSamples() < maxBlock)
        {
            scratch.setSize (2, maxBlock);
            songs.setSize (2, maxBlock);
        }
    }
    void setStream (int slot, PlayStream* stream) noexcept { streams[(size_t) slot] = stream; }

    //==============================================================================
    // Commands (message thread; acted on at the start of the next block)

    /** Starts @p slot, fading in over @p seconds, as the current song. Other voices carry on. */
    void start (int slot, double seconds, float gain);

    /** Fades every voice out over @p seconds and @p slot in over the same time; @p slot becomes current. */
    void crossfadeTo (int slot, double seconds, float gain);

    /** When the current song has @p seconds + @p leadSeconds left, crossfade
        into @p slot over @p seconds (0: right after it, gapless). The lead
        ends the current song early: at its fade-out point (smart crossfade). */
    void arm (int slot, double seconds, float gain, double leadSeconds = 0.0);
    void disarm();

    /** Fades everything out over @p seconds; the voices then let their streams go. */
    void fadeOutAll (double seconds);

    void pause (double seconds);
    void resume (double seconds);

    // Pads (the sound stays alive until a later command has been handled: see isHandled)
    void setPad (int pad, const PadSound*);
    /** Starts @p pad from its beginning, or stops it (a short fade) if it plays. */
    void triggerPad (int pad, float gain, bool loop, bool ducksMusic);
    void stopPads();
    bool isPadPlaying (int pad) const noexcept { return padPlaying[(size_t) pad].load(); }
    float getPadProgress (int pad) const noexcept { return padProgress[(size_t) pad].load(); }

    // Pre-listen, on the cue output
    void cueStart (int slot, float gain);
    void cueStop();
    void setCueLevel (float gain) noexcept { cueLevel = gain; }
    bool isCueing() const noexcept { return cueState.load(); }

    // Ducking
    void setDuck (const DuckSettings& s) noexcept;
    void setManualDuck (bool on) noexcept { manualDuck = on; }
    /** The music's dip now (dB, <= 0). */
    float getDuckDb() const noexcept { return duckShown.load(); }

    void setLevel (float gain) noexcept { level.setTargetValue (gain); }

    /** The safety limiter on the output (on by default, -0.3 dBFS): only for
        sums the analysis can't foresee (two loud songs mid-crossfade, Level up). */
    void setSafetyLimiter (bool on) noexcept { limiterOn = on; }
    static constexpr float kSafetyCeilingDb = -0.3f;

    /** Most gain reduction of the safety limiter since the last call (dB, <= 0). */
    float takeLimiterReductionDb() noexcept { return reduction.exchange (0.0f); }

    //==============================================================================
    // Events (message thread)
    struct Event
    {
        enum class Type { released, advanced, ended, underrun, cueEnded };
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

    /** True once the audio thread has handled every command up to @p sentCount. */
    bool isHandled (juce::uint32 sentCount) const noexcept { return handled.load() >= sentCount; }

    //==============================================================================
    /** Adds the music to @p out (2 channels, cleared by the caller); listens to
        @p key (the duck key, may be nullptr); writes pre-listen to @p cue (may
        be nullptr). Audio thread. */
    void process (float* const* out, int numSamples, const float* const* key = nullptr, int keyChannels = 0,
                  float* const* cue = nullptr) noexcept;

private:
    struct Command
    {
        enum class Type { start, crossfade, arm, disarm, fadeOutAll, pause, resume,
                          setPad, triggerPad, stopPads, cueStart, cueStop };
        Type type = Type::start;
        int slot = -1;              // a slot, or a pad
        double seconds = 0.0;
        float gain = 1.0f;
        double lead = 0.0;
        const PadSound* sound = nullptr;
        bool loop = false, ducks = false;
    };

    struct Pad
    {
        const PadSound* sound = nullptr;
        bool playing = false, loop = false, ducks = false, stopping = false;
        int position = 0;
        float gain = 1.0f;
        double fade = 0.0, fadeStep = 0.0;
    };

    struct Cue
    {
        int slot = -1;
        float gain = 1.0f;
        double fade = 0.0, fadeStep = 0.0;
        bool stopping = false;
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
    double armedSeconds = 0.0, armedLead = 0.0;
    float armedGain = 1.0f;

    double pauseFade = 1.0, pauseStep = 0.0;
    bool paused = false;
    juce::SmoothedValue<float> level { 1.0f };
    rkfx::PeakLimiter limiter;
    std::atomic<bool> limiterOn { true };
    std::atomic<float> reduction { 0.0f };

    juce::AudioBuffer<float> scratch, songs;

    std::array<Pad, kPads> pads;
    std::array<std::atomic<bool>, kPads> padPlaying {};
    std::array<std::atomic<float>, kPads> padProgress {};
    Cue cue;
    std::atomic<bool> cueState { false };
    std::atomic<float> cueLevel { 1.0f };

    // Ducking (settings from the message thread, state on the audio thread)
    std::atomic<bool> duckAuto { false }, manualDuck { false };
    std::atomic<float> duckDepth { -12.0f }, duckThreshold { -35.0f }, duckAttack { 0.2f }, duckHold { 1.0f }, duckRelease { 1.5f };
    std::atomic<float> duckShown { 0.0f };
    double keyEnvelope = 0.0, keyCoef = 0.0;
    double hpX1 = 0.0, hpX2 = 0.0, hpY1 = 0.0, hpY2 = 0.0;   // the key's 100 Hz high-pass
    double hpB0 = 1.0, hpB1 = 0.0, hpB2 = 0.0, hpA1 = 0.0, hpA2 = 0.0;
    bool keyActive = false;
    int holdLeft = 0;
    float duckDb = 0.0f;

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
    void renderPads (float* const* out, int numSamples) noexcept;
    void renderCue (float* const* out, int numSamples) noexcept;
    void duck (float* const* music, int numSamples, const float* const* key, int keyChannels) noexcept;
    void release (int voice) noexcept;
    double stepFor (double seconds) const noexcept { return seconds > 0.0 ? 1.0 / (seconds * sampleRate) : 1.0; }
};
