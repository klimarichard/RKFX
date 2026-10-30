#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MixLayout.h"
#include "MixStrip.h"
#include "safety/Smoothing.h"

#include <array>
#include <cstdint>
#include <utility>

/** The RKMix DSP: 32 input channels (linkable in pairs) into a stereo main
    and its mono sum, each sent to any of the 32 outputs.

        input -> fixed strip (StripDsp) -> mute, fader, pan -> main L/R
        main -> fader, mute, balance -> main output; (L + R) / 2 -> mono fader, mute -> mono output

    A linked pair is one stereo channel with its left channel's settings.
    Every gain glides (faders and pans 20 ms, mutes and switches 10 ms), and
    the strip's sections crossfade on and off. A stereo pair at its
    defaults is a bit-exact copy to the main. Several sources on one output
    add up. A gate or compressor key is another input after its trim.

    GUI-free and allocation-free after prepare(). */
class MixEngine
{
public:
    static constexpr int kChannels = mix::kChannels;
    static constexpr int kPairs = mix::kPairs;
    static constexpr double kMuteFadeSeconds = 0.01;

    struct Channel
    {
        StripDsp::Settings strip;
        float pan = 0.0f;                     // -1 .. +1 (pan of a mono channel, balance of a pair)
        bool toMain = true;
    };

    struct Main
    {
        float balance = 0.0f;                 // -1 .. +1
        int output = mix::pairTarget (0);     // Out 1/2
        int monoOutput = mix::kTargetOff;
    };

    struct Settings
    {
        std::array<Channel, kChannels> channels {};
        std::array<bool, kPairs> links {};
        Main main;
    };

    /** The host-parameter levels, set every block (cheap). */
    struct Levels
    {
        Levels() { channelDb.fill (0.0f); channelMute.fill (false); }
        std::array<float, kChannels> channelDb;
        std::array<bool, kChannels> channelMute;
        float mainDb = 0.0f, monoDb = 0.0f;
        bool mainMute = false, monoMute = false;
    };

    /** Which parts of the settings changed (setSettings only looks at these). */
    static constexpr std::uint64_t kAllChannels = (std::uint64_t (1) << kChannels) - 1;
    static constexpr std::uint64_t kMainBit = std::uint64_t (1) << 63;
    static constexpr std::uint64_t kEverything = ~std::uint64_t (0);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setSettings (const Settings&, std::uint64_t changed = kEverything);
    void setLevels (const Levels&);
    const Settings& getSettings() const noexcept { return settings; }

    /** @param in   the 32 inputs (left untouched)
        @param out  the 32 outputs (overwritten; may not alias @p in)
        @param measure  update the meters (only while a window shows them) */
    void process (const float* const* in, float* const* out, int numSamples, bool measure) noexcept;

    /** The loudest sample since the last call (resets): a channel before its
        fader (both sides of a pair: its two channels), the main after its
        fader, the mono sum. */
    float takeChannelPeak (int channel) noexcept   { return std::exchange (channelPeaks[(size_t) channel], 0.0f); }
    float takeMainPeak (int side) noexcept         { return std::exchange (mainPeaks[(size_t) side], 0.0f); }
    float takeMonoPeak() noexcept                  { return std::exchange (monoPeak, 0.0f); }

    /** A channel's gate and compressor gain reduction since the last call (dB, <= 0). */
    float takeGateReductionDb (int channel) noexcept { return states[(size_t) channel].strip.takeGateReductionDb(); }
    float takeCompReductionDb (int channel) noexcept { return states[(size_t) channel].strip.takeCompReductionDb(); }

private:
    struct ChannelState
    {
        StripDsp strip;
        rkfx::Smoothed fader, mute, pan;
        rkfx::Smoothed toMain;                        // the Main switch, faded
    };

    double sampleRate = 48000.0;
    int maxBlock = 0;
    Settings settings;
    Levels levels;

    std::array<ChannelState, kChannels> states;
    rkfx::Smoothed mainFader, mainMute, mainBalance, monoFader, monoMute;

    juce::AudioBuffer<float> channelBuffer, scratchBuffer, postBuffer, mainBuffer, monoBuffer, keyBuffer;

    std::array<float, kChannels> channelPeaks {};
    std::array<float, 2> mainPeaks {};
    float monoPeak = 0.0f;

    bool isStereo (int ch) const noexcept { return ch % 2 == 0 && settings.links[(size_t) (ch / 2)]; }
    bool isFollower (int ch) const noexcept { return ch % 2 == 1 && settings.links[(size_t) (ch / 2)]; }

    void applyChannel (int ch, bool snap);
    void applyMain (bool snap);
    void resetChannel (int ch);

    void processChannel (int ch, const float* const* in, int numSamples, bool measure) noexcept;
    void processMain (float* const* out, int numSamples, bool measure) noexcept;

    /** A key's signal (input @p key after its trim) in keyBuffer channel @p slot, or nullptr for none. */
    const float* keySignal (int key, int slot, const float* const* in, int numSamples) noexcept;

    /** Adds a stereo @p source to @p bus through a faded on/off switch. */
    static void addSwitched (rkfx::Smoothed& gate, juce::AudioBuffer<float>& bus, const float* const* source, int numSamples) noexcept;

    /** Adds a mono (@p right == nullptr) or stereo signal to an output target. */
    static void addToTarget (float* const* out, int target, const float* left, const float* right, int numSamples) noexcept;
};
