#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MixBusDsp.h"
#include "MixDelay.h"
#include "MixLayout.h"
#include "MixStrip.h"
#include "safety/Smoothing.h"

#include <array>
#include <cstdint>
#include <utility>

/** The RKMix DSP: 32 input channels (linkable in pairs), 16 buses, a stereo
    main and its mono sum, on any of the 32 outputs.

        input -> fixed strip (StripDsp) -> mute, fader, pan -> main L/R
                 |    each send taps the strip at its own point ->  Aux buses
                 +-> post-fader: Group buses (assigned), FX buses (sends)
        Group buses (in number order) -> main, later groups, Aux and FX buses
        FX buses -> main, Aux buses
        Aux buses -> their outputs
        main -> fader, mute, balance -> main output; (L + R) / 2 -> mono sum

    A linked pair is one stereo channel with its left channel's settings.
    Every gain glides (faders, pans and sends 20 ms, mutes and switches
    10 ms), and the strip's sections crossfade on and off. Mute applies at
    every tap. A mono channel into a stereo bus is panned (-3 dB at the
    centre), into a mono bus it goes as it is; a stereo channel into a mono
    bus is (L + R) / 2. A stereo pair at its defaults is a bit-exact copy to
    the main. Several sources on one output add up.

    Inserts (the processor's RKFX plugins) run where an Inserts object
    says: after a channel's strip, before a bus's or the main's fader, on the
    mono sum. When some report latency, every path is lined up where signals
    meet: channels with less latency are delayed to the slowest (sends taken
    before the inserts too), each bus's channel inputs and bus inputs are
    delayed to arrive together, and every output is delayed to the same total,
    which getLatency() reports. With no latency nothing is delayed.

    GUI-free and allocation-free after prepare(). */
class MixEngine
{
public:
    static constexpr int kChannels = mix::kChannels;
    static constexpr int kPairs = mix::kPairs;
    static constexpr int kBuses = mix::kBuses;
    static constexpr double kMuteFadeSeconds = 0.01;
    static constexpr int kMaxChannelLatency = 512;     // samples, a channel's inserts (lined up to the slowest)
    static constexpr int kMaxPathLatency = 4096;       // samples, any path to an output

    /** The inserts, run by the engine where they sit (implemented by the processor). */
    class Inserts
    {
    public:
        enum class Strip { channel, bus, main, mono };
        virtual ~Inserts() = default;

        /** Runs the inserts of channel / bus @p index (or the main, mono) in place. */
        virtual void process (Strip, int index, float* const* io, int numSides, int numSamples) noexcept = 0;

        /** True when the strip has an insert that must run even on silence (a reverb's tail). */
        virtual bool isActive (Strip, int index) const noexcept = 0;
    };

    /** The latency the inserts of each strip add (leaders of linked pairs only). */
    struct Latencies
    {
        std::array<int, kChannels> channels {};
        std::array<int, kBuses> buses {};
        int main = 0, mono = 0;
    };

    struct Channel
    {
        Channel() { sendDb.fill (mix::kFaderMinDb); tap.fill (-1); assign.fill (false); }
        StripDsp::Settings strip;
        float pan = 0.0f;                         // -1 .. +1 (pan of a mono channel, balance of a pair)
        bool toMain = true;
        std::array<float, kBuses> sendDb;         // to Aux and FX buses (fader law)
        std::array<int, kBuses> tap;              // Aux sends: a Tap, or -1 = the bus's default
        std::array<bool, kBuses> assign;          // to Group buses
    };

    struct Bus
    {
        Bus() { sendDb.fill (mix::kFaderMinDb); assign.fill (false); }
        mix::BusType type = mix::BusType::aux;
        bool stereo = true;
        int output = mix::kTargetOff;
        bool toMain = true;                       // Group and FX
        mix::Tap defaultTap = mix::Tap::preFader; // Aux
        mix::PanMode panMode = mix::PanMode::follow;   // Aux: how the channels sit in it
        BusDsp::Settings dsp;                     // EQ, compressor, limiter
        float pan = 0.0f;                         // into the main (and stereo buses it feeds)
        std::array<float, kBuses> sendDb;         // Group -> Aux/FX, FX -> Aux
        std::array<bool, kBuses> assign;          // Group -> later Groups
    };

    struct Main
    {
        float balance = 0.0f;                     // -1 .. +1
        int output = mix::pairTarget (0);         // Out 1/2
        int monoOutput = mix::kTargetOff;
        BusDsp::Settings dsp, monoDsp;            // the main's and the mono sum's EQ, compressor, limiter
    };

    struct Settings
    {
        std::array<Channel, kChannels> channels {};
        std::array<bool, kPairs> links {};
        std::array<Bus, kBuses> buses {};
        Main main;
    };

    /** The host-parameter levels, set every block (cheap). */
    struct Levels
    {
        Levels() { channelDb.fill (0.0f); channelMute.fill (false); busDb.fill (0.0f); busMute.fill (false); }
        std::array<float, kChannels> channelDb;
        std::array<bool, kChannels> channelMute;
        std::array<float, kBuses> busDb;
        std::array<bool, kBuses> busMute;
        float mainDb = 0.0f, monoDb = 0.0f;
        bool mainMute = false, monoMute = false;
    };

    /** Which parts of the settings changed (setSettings only looks at these):
        bit n = channel n, 32 + n = bus n, 63 = the main and the links. */
    static constexpr std::uint64_t kMainBit = std::uint64_t (1) << 63;
    static constexpr std::uint64_t kEverything = ~std::uint64_t (0);
    static constexpr std::uint64_t busBit (int bus) noexcept { return std::uint64_t (1) << (32 + bus); }

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setSettings (const Settings&, std::uint64_t changed = kEverything);
    void setLevels (const Levels&);
    const Settings& getSettings() const noexcept { return settings; }

    /** The inserts to run (nullptr: none). Set before processing. */
    void setInserts (Inserts* i) noexcept { inserts = i; }

    /** The inserts' latencies; the paths are lined up again from the next block. */
    void setLatencies (const Latencies&);

    /** The latency of every output (samples): the slowest path's. */
    int getLatency() const noexcept { return plan.total; }

    /** @param in   the 32 inputs (left untouched)
        @param out  the 32 outputs (overwritten; may not alias @p in)
        @param measure  update the meters (only while a window shows them) */
    void process (const float* const* in, float* const* out, int numSamples, bool measure) noexcept;

    /** The loudest sample since the last call (resets): a channel before its
        fader (both sides of a pair: its two channels), a bus, the main after
        their faders, the mono sum. */
    float takeChannelPeak (int channel) noexcept   { return std::exchange (channelPeaks[(size_t) channel], 0.0f); }
    float takeBusPeak (int bus, int side) noexcept { return std::exchange (busPeaks[(size_t) bus][(size_t) side], 0.0f); }
    float takeMainPeak (int side) noexcept         { return std::exchange (mainPeaks[(size_t) side], 0.0f); }
    float takeMonoPeak() noexcept                  { return std::exchange (monoPeak, 0.0f); }

    /** A bus's (or the main's, the mono sum's) compressor and limiter gain reduction since the last call (dB, <= 0). */
    float takeBusCompReductionDb (int bus) noexcept  { return busStates[(size_t) bus].dsp.takeCompReductionDb(); }
    float takeBusLimitReductionDb (int bus) noexcept { return busStates[(size_t) bus].dsp.takeLimitReductionDb(); }
    BusDsp& getMainDsp() noexcept { return mainDsp; }
    BusDsp& getMonoDsp() noexcept { return monoDsp; }

    /** A channel's gate and compressor gain reduction since the last call (dB, <= 0). */
    float takeGateReductionDb (int channel) noexcept { return states[(size_t) channel].strip.takeGateReductionDb(); }
    float takeCompReductionDb (int channel) noexcept { return states[(size_t) channel].strip.takeCompReductionDb(); }

    /** True when a Group bus @p from may feed bus @p to (a later Group), or a bus may send to it. */
    static bool canAssign (const Settings&, int from, int to) noexcept;
    static bool canSend (const Settings&, int from, int to) noexcept;

private:
    /** A gain over one block: steady (a value) or gliding (one per sample). */
    struct Curve
    {
        const float* data = nullptr;
        float value = 1.0f;
        float at (int i) const noexcept { return data != nullptr ? data[i] : value; }
        bool steady() const noexcept { return data == nullptr; }
    };

    struct ChannelState
    {
        StripDsp strip;
        rkfx::Smoothed fader, mute, pan;
        rkfx::Smoothed toMain;                        // the Main switch, faded
        std::array<rkfx::Smoothed, kBuses> sends;     // linear send gains
        std::array<rkfx::Smoothed, kBuses> assigns;   // group assigns, faded
    };

    struct BusState
    {
        BusDsp dsp;
        rkfx::Smoothed fader, mute, pan, toMain;
        std::array<rkfx::Smoothed, kBuses> sends, assigns;
    };

    /** Where each path is lined up (see computePlan). */
    struct Plan
    {
        bool active = false;                          // some insert has latency
        int channelLatency = 0;                       // every channel's, after lining up
        std::array<int, kChannels> channelPad {};     // added after a channel's inserts
        std::array<int, kBuses> busInput {};          // when a bus's inputs meet
        std::array<int, kBuses> busLatency {};        // a bus's output
        std::array<int, kBuses> busOutputPad {};
        int mainInput = 0, mainLatency = 0, mainOutputPad = 0, monoOutputPad = 0;
        int total = 0;
    };

    // Rows of the per-block gain buffer
    enum Row { muteRow, faderRow, panLeftRow, panRightRow, productRow, monoRow, numRows };

    double sampleRate = 48000.0;
    int maxBlock = 0;
    Settings settings;
    Levels levels;

    std::array<ChannelState, kChannels> states;
    std::array<BusState, kBuses> busStates;
    rkfx::Smoothed mainFader, mainMute, mainBalance, monoFader, monoMute;
    BusDsp mainDsp, monoDsp;

    juce::AudioBuffer<float> channelBuffer, scratchBuffer, postBuffer, mainBuffer, monoBuffer, keyBuffer, curveBuffer;
    juce::AudioBuffer<float> busBuffers;              // 2 channels per bus
    juce::AudioBuffer<float> busChannelBuffers;       // the channels' part, lined up separately while a plan is active
    juce::AudioBuffer<float> mainChannelBuffer, delayScratch;

    Inserts* inserts = nullptr;
    Latencies latencies;
    Plan plan;
    std::array<std::array<BlockDelay, 2>, kChannels> channelPadLines;
    std::array<std::array<std::array<BlockDelay, 2>, 4>, kChannels> channelTapLines;   // the taps before the inserts
    std::array<std::array<BlockDelay, 2>, kBuses> busChannelLines, busHistory;
    std::array<BlockDelay, 2> mainChannelLines, mainPadLines;
    BlockDelay monoPadLine;
    std::array<bool, kBuses> busFed {};               // something was added to the bus this block

    std::array<float, kChannels> channelPeaks {};
    std::array<std::array<float, 2>, kBuses> busPeaks {};
    std::array<float, 2> mainPeaks {};
    float monoPeak = 0.0f;

    bool isStereo (int ch) const noexcept { return ch % 2 == 0 && settings.links[(size_t) (ch / 2)]; }
    bool isFollower (int ch) const noexcept { return ch % 2 == 1 && settings.links[(size_t) (ch / 2)]; }
    float* busData (int bus, int side) noexcept { return busBuffers.getWritePointer (bus * 2 + side); }
    float* busChannelData (int bus, int side) noexcept { return busChannelBuffers.getWritePointer (bus * 2 + side); }
    bool feeds (int from, int to) const noexcept;
    void computePlan() noexcept;
    mix::Tap tapOf (int ch, int bus) const noexcept;

    void applyChannel (int ch, bool snap);
    void applyBus (int bus, bool snap);
    void applyMain (bool snap);
    void resetChannel (int ch);

    void processChannel (int ch, const float* const* in, int numSamples, bool measure) noexcept;
    void processBus (int bus, float* const* out, int numSamples, bool measure) noexcept;
    void processMain (float* const* out, int numSamples, bool measure) noexcept;

    /** This block's active sends of one channel: bus and tap, in bus order. */
    struct ActiveSends
    {
        std::array<std::pair<int, mix::Tap>, kBuses> sends {};
        int count = 0;
        bool uses (mix::Tap tap) const noexcept
        {
            for (int n = 0; n < count; ++n)
                if (sends[(size_t) n].second == tap)
                    return true;
            return false;
        }
    };

    /** The channel's sends taken at @p tap (the Aux sends set there; post-fader: the FX sends too). */
    void sendAtTap (int ch, const ActiveSends&, mix::Tap, const float* const* io, int numSides, const Curve& mute,
                    const Curve& fader, const Curve& panLeft, const Curve& panRight, int numSamples) noexcept;

    /** Adds a mono or stereo source to bus @p bus: into a stereo bus panned by
        @p panLeft/@p panRight, into a mono bus summed, times every gain. A
        channel's contribution (@p fromChannel) is kept apart while a plan is
        active, to be lined up with the bus's other inputs. */
    void addToBus (int bus, const float* const* source, int numSides, const Curve& panLeft, const Curve& panRight,
                   std::initializer_list<const Curve*> gains, int numSamples, bool fromChannel = true) noexcept;

    /** A bus's output @p delay samples later (its history), in delayScratch; or the output itself. */
    void delayedBusOutput (int bus, float* const* io, int numSides, int delay, const float* (&result)[2], int numSamples) noexcept;

    Curve curveOf (rkfx::Smoothed&, Row, int numSamples) noexcept;
    void panCurves (rkfx::Smoothed& pan, bool stereoSource, Curve& left, Curve& right, int numSamples) noexcept;
    static Curve constant (float v) noexcept { return { nullptr, v }; }

    /** dest += source x the product of @p gains. */
    void accumulate (float* dest, const float* source, std::initializer_list<const Curve*> gains, int numSamples) noexcept;

    /** A key's signal (input @p key after its trim) in keyBuffer channel @p slot, or nullptr for none. */
    const float* keySignal (int key, int slot, const float* const* in, int numSamples) noexcept;

    /** Adds a mono (@p right == nullptr) or stereo signal to an output target. */
    static void addToTarget (float* const* out, int target, const float* left, const float* right, int numSamples) noexcept;
};
