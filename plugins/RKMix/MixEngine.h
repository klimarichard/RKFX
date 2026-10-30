#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MixLayout.h"
#include "dsp/DelayLine.h"
#include "safety/Smoothing.h"

#include <array>
#include <cstdint>
#include <utility>

/** The RKMix DSP: 32 input channels (linkable in pairs) into a stereo main
    and its mono sum, each sent to any of the 32 outputs.

        input -> trim, polarity -> delay -> mute, fader, pan -> main L/R
        main -> fader, mute, balance -> main output; (L + R) / 2 -> mono fader, mute -> mono output

    A linked pair is one stereo channel with its left channel's settings.
    Every gain glides (faders and pans 20 ms, mutes 10 ms), polarity flips
    fade through silence, and switching a section on or off crossfades over
    20 ms; an off section is skipped. A stereo pair at its defaults is a
    bit-exact copy to the main. Several sources on one output add up.

    GUI-free and allocation-free after prepare(). */
class MixEngine
{
public:
    static constexpr int kChannels = mix::kChannels;
    static constexpr int kPairs = mix::kPairs;
    static constexpr float kMaxDelayMs = 20.0f;
    static constexpr double kSectionFadeSeconds = 0.02;
    static constexpr double kMuteFadeSeconds = 0.01;
    static constexpr double kDelayGlideSeconds = 0.05;

    struct Channel
    {
        float trimDb = 0.0f;
        std::array<bool, 2> polarity {};     // left, right (a linked pair)
        bool delayOn = false;
        float delayMs = 0.0f;
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

private:
    /** A section switched on and off with a crossfade. */
    struct SectionFade
    {
        rkfx::Smoothed amount;
        bool isOff() const noexcept  { return ! amount.isSmoothing() && amount.getTargetValue() <= 0.0f; }
        bool isFull() const noexcept { return ! amount.isSmoothing() && amount.getTargetValue() >= 1.0f; }
        /** Returns true when the section wakes up from fully off (its state needs a reset). */
        bool set (bool on) noexcept
        {
            const auto wakes = on && isOff();
            amount.setTargetValue (on ? 1.0f : 0.0f);
            return wakes;
        }
    };

    struct ChannelState
    {
        std::array<rkfx::Smoothed, 2> input;          // trim x polarity, per side
        SectionFade delayFade;
        rkfx::Smoothed delaySamples;
        std::array<rkfx::DelayLine, 2> delay;
        rkfx::Smoothed fader, mute, pan;
        rkfx::Smoothed toMain;                        // the Main switch, faded
    };

    double sampleRate = 48000.0;
    int maxBlock = 0;
    Settings settings;
    Levels levels;

    std::array<ChannelState, kChannels> states;
    rkfx::Smoothed mainFader, mainMute, mainBalance, monoFader, monoMute;

    juce::AudioBuffer<float> channelBuffer, dryBuffer, postBuffer, gainBuffer, mainBuffer, monoBuffer;

    std::array<float, kChannels> channelPeaks {};
    std::array<float, 2> mainPeaks {};
    float monoPeak = 0.0f;

    bool isStereo (int ch) const noexcept { return ch % 2 == 0 && settings.links[(size_t) (ch / 2)]; }
    bool isFollower (int ch) const noexcept { return ch % 2 == 1 && settings.links[(size_t) (ch / 2)]; }

    void applyChannel (int ch, bool snap);
    void applyMain (bool snap);
    void resetChannel (int ch);

    void processChannel (int ch, const float* const* in, int numSamples, bool measure) noexcept;
    void processDelay (ChannelState&, float* const* io, int numSides, int numSamples) noexcept;
    void processMain (float* const* out, int numSamples, bool measure) noexcept;

    /** Adds a stereo @p source to @p bus through a faded on/off switch. */
    static void addSwitched (rkfx::Smoothed& gate, juce::AudioBuffer<float>& bus, const float* const* source, int numSamples) noexcept;

    /** Crossfades a section in and out: @p process works in place on @p io. */
    template <typename Process>
    void runSection (SectionFade&, float* const* io, int numSides, int numSamples, Process&& process) noexcept;

    /** Adds a mono (@p right == nullptr) or stereo signal to an output target. */
    static void addToTarget (float* const* out, int target, const float* left, const float* right, int numSamples) noexcept;
};
