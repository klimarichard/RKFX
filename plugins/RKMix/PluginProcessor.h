#pragma once

#include "plugin/ProcessorBase.h"

#include "MixEngine.h"
#include "MixParams.h"
#include "MixSettings.h"

#include <array>
#include <atomic>

/** RKMix: a mixing console. 32 inputs (linkable in pairs) into channel
    strips, a stereo main and a mono sum, on any of the 32 outputs.

    The channel, bus, main and mono faders and mutes are host parameters;
    everything else is a mix setting (MixSettings), saved with the session
    and in presets, with the channels' names and colours. A change to a
    setting only rebuilds the part of the mix it belongs to. */
class RKMixProcessor : public rkfx::ProcessorBase
{
public:
    static constexpr int kChannels = mix::kChannels;
    static constexpr int kNumColours = 8;

    RKMixProcessor();
    ~RKMixProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    MixSettings& getSettings() noexcept { return settings; }
    const MixSettings& getSettings() const noexcept { return settings; }

    /** Channel names and colours (message thread). Saved with the mix. */
    juce::String getChannelName (int channel) const;
    void setChannelName (int channel, const juce::String&);
    int getChannelColour (int channel) const noexcept { return channelColours[(size_t) channel].load(); }
    void setChannelColour (int channel, int colour);

    /** Bumped when names or colours change (the editor polls it). */
    int getNamesVersion() const noexcept { return namesVersion.load(); }

    bool isLinked (int pair) const { return settings.flag (mix::linkId (pair)); }

    /** The loudest sample since the last call (message thread, for the meters). */
    float takeChannelPeak (int channel) noexcept { return channelPeaks[(size_t) channel].exchange (0.0f); }
    float takeMainPeak (int side) noexcept       { return mainPeaks[(size_t) side].exchange (0.0f); }
    float takeMonoPeak() noexcept                { return monoPeak.exchange (0.0f); }
    /** A channel's most gate or compressor gain reduction since the last call (dB, <= 0). */
    float takeGateReduction (int channel) noexcept { return gateReduction[(size_t) channel].exchange (0.0f); }
    float takeCompReduction (int channel) noexcept { return compReduction[(size_t) channel].exchange (0.0f); }

    /** Set while a window shows the meters (no measuring otherwise). */
    std::atomic<int> windowsOpen { 0 };

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

    juce::var getExtraState() const override;
    void setExtraState (const juce::var&) override;

private:
    MixSettings settings;

    /** A channel's settings, by field (see kChannelFields in the .cpp). */
    enum ChannelField
    {
        trim, pol, polr, delayOn, delay, pan, toMain,
        hpfOn, hpf, hpfSlope,
        gateOn, gateThr, gateRange, gateAtt, gateHold, gateRel, gateKey,
        eqOn, eqLowType, eqHighType, eqBands,                       // eqBands + 3 * band + (0 freq, 1 gain, 2 q)
        compOn = eqBands + 12, compThr, compRatio, compKnee, compAtt, compRel, compMakeup, compKey,
        numChannelFields
    };
    using ChannelParams = std::array<juce::RangedAudioParameter*, numChannelFields>;
    std::array<ChannelParams, kChannels> channelParams {};
    std::array<juce::RangedAudioParameter*, mix::kPairs> linkParams {};
    juce::RangedAudioParameter *mainBalance = nullptr, *mainOutput = nullptr, *monoOutput = nullptr;

    std::array<juce::RangedAudioParameter*, kChannels> channelFaders {}, channelMutes {};
    juce::RangedAudioParameter *mainFader = nullptr, *mainMute = nullptr, *monoFader = nullptr, *monoMute = nullptr;

    MixEngine engine;
    MixEngine::Settings engineSettings;                 // audio thread
    std::atomic<std::uint64_t> changed { MixEngine::kEverything };
    std::atomic<bool> loadingMix { false };

    void readSettings (std::uint64_t parts);            // mix settings -> engineSettings
    MixEngine::Levels readLevels() const;

    mutable juce::CriticalSection namesLock;
    std::array<juce::String, kChannels> channelNames;
    std::array<std::atomic<int>, kChannels> channelColours {};
    std::atomic<int> namesVersion { 0 };

    std::array<std::atomic<float>, kChannels> channelPeaks {};
    std::array<std::atomic<float>, 2> mainPeaks {};
    std::atomic<float> monoPeak { 0.0f };
    std::array<std::atomic<float>, kChannels> gateReduction {}, compReduction {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKMixProcessor)
};
