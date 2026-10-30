#pragma once

#include "plugin/ProcessorBase.h"

#include "MixEngine.h"
#include "MixInserts.h"
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

    /** Bus and output names, bus colours (message thread). Saved with the mix. */
    juce::String getBusName (int bus) const;
    void setBusName (int bus, const juce::String&);
    int getBusColour (int bus) const noexcept { return busColours[(size_t) bus].load(); }
    void setBusColour (int bus, int colour);
    juce::String getOutputName (int output) const;
    void setOutputName (int output, const juce::String&);

    mix::BusType getBusType (int bus) const { return (mix::BusType) settings.choice (mix::busId (bus, "type")); }
    bool isBusStereo (int bus) const { return settings.choice (mix::busId (bus, "format")) == 1; }

    /** What may feed what (by the buses' current types): a group into a later
        group, a group into Aux and FX buses, an FX bus into Aux buses. */
    bool canAssign (int from, int to) const
    {
        return to > from && getBusType (from) == mix::BusType::group && getBusType (to) == mix::BusType::group;
    }
    bool canSend (int from, int to) const
    {
        const auto source = getBusType (from), dest = getBusType (to);
        return from != to && ((source == mix::BusType::group && dest != mix::BusType::group)
                              || (source == mix::BusType::fx && dest == mix::BusType::aux));
    }

    /** Bumped when names or colours change (the editor polls it). */
    int getNamesVersion() const noexcept { return namesVersion.load(); }

    bool isLinked (int pair) const { return settings.flag (mix::linkId (pair)); }

    /** Channel @p ch's settings as the engine sees them (any thread; for the displays). */
    MixEngine::Channel channelSettings (int ch) const;

    /** A bus's, the main's or the mono sum's EQ, compressor and limiter (mix::kMainChain, kMonoChain). */
    BusDsp::Settings chainSettings (int chain) const;

    /** A chain's compressor and limiter gain reduction since the last call (dB, <= 0; for the meters). */
    float takeChainCompReduction (int chain) noexcept  { return chainCompReduction[(size_t) chain].exchange (0.0f); }
    float takeChainLimitReduction (int chain) noexcept { return chainLimitReduction[(size_t) chain].exchange (0.0f); }

    /** The inserts (message thread, except what InsertRack says). */
    InsertRack& getRack() noexcept { return rack; }

    /** How many channels strip @p index of @p strip has (1 or 2): its inserts run so. */
    int channelsOf (InsertRack::Strip, int index) const;

    /** Channel presets: the fixed strip, its inserts, name and colour (message thread). */
    juce::File getChannelPresetFolder();
    PresetResult saveChannelPreset (int channel, const juce::File&);
    PresetResult loadChannelPreset (int channel, const juce::File&);
    void resetChannelStrip (int channel);

    /** The loudest sample since the last call (message thread, for the meters). */
    float takeChannelPeak (int channel) noexcept { return channelPeaks[(size_t) channel].exchange (0.0f); }
    float takeMainPeak (int side) noexcept       { return mainPeaks[(size_t) side].exchange (0.0f); }
    float takeMonoPeak() noexcept                { return monoPeak.exchange (0.0f); }
    float takeBusPeak (int bus, int side) noexcept { return busPeaks[(size_t) bus][(size_t) side].exchange (0.0f); }
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
    std::array<juce::RangedAudioParameter*, mix::kBuses> busFaders {}, busMutes {};

    /** A channel's sends, taps and group assigns; a bus's settings, sends and assigns. */
    struct RoutingParams
    {
        std::array<juce::RangedAudioParameter*, mix::kBuses> sends {}, taps {}, assigns {};
    };
    std::array<RoutingParams, kChannels> channelRouting {};
    struct BusParams
    {
        juce::RangedAudioParameter *type = nullptr, *format = nullptr, *output = nullptr, *toMain = nullptr,
                                   *tap = nullptr, *panMode = nullptr, *pan = nullptr;
        RoutingParams routing;
    };
    std::array<BusParams, mix::kBuses> busParams {};
    std::array<std::vector<juce::RangedAudioParameter*>, mix::kProcessingChains> chainParams;   // by MixParams::chainFields()
    juce::RangedAudioParameter *mainFader = nullptr, *mainMute = nullptr, *monoFader = nullptr, *monoMute = nullptr;

    InsertRack rack;
    MixEngine engine;
    MixEngine::Settings engineSettings;                 // audio thread

    // The inserts' latencies, handed to the audio thread.
    juce::SpinLock latencyLock;
    MixEngine::Latencies pendingLatencies;
    std::atomic<bool> latenciesChanged { true };
    int reportedLatency = -1;                           // audio thread
    void updateLatencies();                             // message thread
    void syncInsertLayouts();                           // strips that changed between mono and stereo
    void matchBusOutputs();                             // a mono bus on one output, a stereo bus on a pair

    struct LayoutSync : public juce::AsyncUpdater
    {
        explicit LayoutSync (RKMixProcessor& p) : owner (p) {}
        void handleAsyncUpdate() override { owner.syncInsertLayouts(); }
        RKMixProcessor& owner;
    };
    LayoutSync layoutSync { *this };
    std::atomic<std::uint64_t> changed { MixEngine::kEverything };
    std::atomic<bool> loadingMix { false };

    void readSettings (std::uint64_t parts);            // mix settings -> engineSettings
    static bool isStripField (int field);               // in channel presets (not routing)
    std::vector<rkfx::ParamInfo> stripInfos() const;
    MixEngine::Levels readLevels() const;

    mutable juce::CriticalSection namesLock;
    std::array<juce::String, kChannels> channelNames, outputNames;
    std::array<juce::String, mix::kBuses> busNames;
    std::array<std::atomic<int>, kChannels> channelColours {};
    std::array<std::atomic<int>, mix::kBuses> busColours {};
    std::atomic<int> namesVersion { 0 };

    std::array<std::atomic<float>, kChannels> channelPeaks {};
    std::array<std::atomic<float>, 2> mainPeaks {};
    std::array<std::array<std::atomic<float>, 2>, mix::kBuses> busPeaks {};
    std::atomic<float> monoPeak { 0.0f };
    std::array<std::atomic<float>, kChannels> gateReduction {}, compReduction {};
    std::array<std::atomic<float>, mix::kProcessingChains> chainCompReduction {}, chainLimitReduction {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKMixProcessor)
};
