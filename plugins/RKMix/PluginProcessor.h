#pragma once

#include "plugin/ProcessorBase.h"

#include "MixEngine.h"
#include "MixAssigns.h"
#include "MixInserts.h"
#include "MixScenes.h"
#include "host/MidiMap.h"
#include "host/Tempo.h"
#include "MixParams.h"
#include "MixSettings.h"

#include <array>
#include <atomic>

/** What a MIDI controller can be learned to in the standalone app. */
namespace MixLearn
{
    constexpr int firstAssign = 0;       // + slot (0..31): continuous
    constexpr int scene = 200;           // the CC value picks the scene (0..15)
    constexpr int tap = 201;             // switch: tap tempo
    constexpr int talk = 202;            // switch: talkback

    juce::String key (int target);       // "assign3", "scene", "tap", "talk"
    int fromKey (const juce::String&);   // -1 when unknown
    juce::String describe (int target);  // "Assign 3", "Scene", ...
    bool isSwitch (int target);
}

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

    /** User pages: the strips each one shows, in the order chosen; a channel
        (a linked pair's left channel), or kUserPageBus + a bus. Up to 16 per
        page (message thread). Saved with the mix; a scene leaves them alone. */
    static constexpr int kUserPages = 2, kUserPageStrips = 16, kUserPageBus = 100;
    juce::Array<int> getUserPage (int page) const;
    void setUserPage (int page, const juce::Array<int>&);

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

    /** Called before an insert is deleted (the editor closes its editor). */
    std::function<void (rkfx::ProcessorBase*)> insertAboutToRemove;

    /** Scenes and assignables (message thread). Recalling moves the Scene host parameter too. */
    MixScenes& getScenes() noexcept { return *scenes; }
    MixAssigns& getAssigns() noexcept { return *assigns; }
    void recallScene (int scene);
    /** "Vocal: Fader", "Bus 2, insert 1: Time": an assignment's control. */
    juce::String describeTarget (const MixAssigns::Target&) const;

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

    /** Solo (message thread, read by the audio thread): never saved, cleared when a mix loads.
        A linked pair solos as its left channel. */
    bool isChannelSoloed (int ch) const noexcept { return channelSolo[(size_t) ch].load(); }
    bool isBusSoloed (int bus) const noexcept    { return busSolo[(size_t) bus].load(); }
    void setChannelSolo (int ch, bool);
    void setBusSolo (int bus, bool);
    void clearSolo();
    bool isAnythingSoloed() const noexcept;
    int getSoloVersion() const noexcept { return soloVersion.load(); }

    float takeCuePeak (int side) noexcept { return cuePeaks[(size_t) side].exchange (0.0f); }

    //==============================================================================
    // Standalone only (in a host, Gig Performer does these): MIDI learn,
    // program changes for scenes, tempo with tap and MIDI clock.

    bool isStandalone() const noexcept { return standalone; }
    /** Tests: behave as the standalone app, with the rig's files in @p rigFolder. */
    void setStandaloneForTesting (const juce::File& rigFolder);

    rkfx::MidiMap* getMidiMap() noexcept { return midiMap.get(); }
    /** The next MIDI controller moved is learned for @p target (a MixLearn target). */
    void startLearning (int target) { learning = target; }
    void cancelLearning() { learning = -1; }
    int getLearningTarget() const noexcept { return learning.load(); }
    /** Bumped when a controller was learned (the editor polls it). */
    int getLearnVersion() const noexcept { return learnVersion.load(); }

    /** The mix's tempo (saved with it; the inserts' tempo in the standalone app). */
    double getTempo() const noexcept { return tempo.load(); }
    void setTempo (double bpm);
    void tapTempo();
    bool isFollowingClock() const noexcept { return followClock.load(); }
    void setFollowClock (bool);

    /** Set while a window shows the meters (no measuring otherwise). */
    std::atomic<int> windowsOpen { 0 };

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override;
    void processDsp (juce::AudioBuffer<float>&) override;

    juce::var getExtraState() const override;
    void setExtraState (const juce::var&) override;
    void releaseMidi (juce::MidiBuffer&) override;

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
        directOut, directTap,
        numChannelFields
    };
    using ChannelParams = std::array<juce::RangedAudioParameter*, numChannelFields>;
    std::array<ChannelParams, kChannels> channelParams {};
    std::array<juce::RangedAudioParameter*, mix::kPairs> linkParams {};
    juce::RangedAudioParameter *mainBalance = nullptr, *mainOutput = nullptr, *monoOutput = nullptr, *midiThru = nullptr;
    juce::MidiBuffer insertMidi;                        // audio thread: what the inserts sent this block

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
    void matchDirectOutputs();                          // the same for channels' direct outputs, as pairs link

    struct LayoutSync : public juce::AsyncUpdater
    {
        explicit LayoutSync (RKMixProcessor& p) : owner (p) {}
        void handleAsyncUpdate() override { owner.syncInsertLayouts(); }
        RKMixProcessor& owner;
    };
    LayoutSync layoutSync { *this };

    // Standalone
    bool standalone = false;
    std::unique_ptr<rkfx::MidiMap> midiMap;
    juce::File rigFolder;
    rkfx::StandalonePlayHead standalonePlayHead;
    rkfx::TapTempo tapper;
    rkfx::MidiClockFollower clock;
    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> followClock { false };
    double sampleTime = 0.0;
    std::atomic<int> learning { -1 }, learnVersion { 0 };
    int learnedTarget = -1, learnedChannel = 0, learnedCC = 0;    // audio thread -> message thread
    std::array<std::atomic<float>, MixAssigns::kNumAssigns> ccTargets {};
    std::array<std::atomic<bool>, MixAssigns::kNumAssigns> ccGliding {};
    std::array<bool, 256> switchHeld {};
    void handleStandaloneMidi (int numSamples);
    void applyController (int target, bool momentary, int value, double time);
    void glideControllers (int numSamples);
    void loadRig();
    void saveRig() const;

    struct LearnNotifier : public juce::AsyncUpdater
    {
        explicit LearnNotifier (RKMixProcessor& p) : owner (p) {}
        void handleAsyncUpdate() override;
        RKMixProcessor& owner;
    };
    LearnNotifier learnNotifier { *this };

    std::unique_ptr<MixScenes> scenes;
    std::unique_ptr<MixAssigns> assigns;
    juce::RangedAudioParameter* sceneParam = nullptr;
    std::atomic<int> pendingScene { -1 };

    /** The Scene host parameter moved (any thread): the recall happens on the message thread. */
    struct SceneSync : public juce::AsyncUpdater,
                       public juce::AudioProcessorParameter::Listener
    {
        explicit SceneSync (RKMixProcessor& p) : owner (p) {}
        void handleAsyncUpdate() override;
        void parameterValueChanged (int, float) override;
        void parameterGestureChanged (int, bool) override {}
        RKMixProcessor& owner;
    };
    SceneSync sceneSync { *this };
    std::atomic<std::uint64_t> changed { MixEngine::kEverything };
    std::atomic<bool> loadingMix { false };

    void readSettings (std::uint64_t parts);            // mix settings -> engineSettings
    static bool isStripField (int field);               // in channel presets (not routing)
    std::vector<rkfx::ParamInfo> stripInfos() const;
    MixEngine::Levels readLevels() const;

    mutable juce::CriticalSection namesLock;
    std::array<juce::String, kChannels> channelNames, outputNames;
    std::array<juce::String, mix::kBuses> busNames;
    std::array<juce::Array<int>, kUserPages> userPages;
    std::array<std::atomic<int>, kChannels> channelColours {};
    std::array<std::atomic<int>, mix::kBuses> busColours {};
    std::atomic<int> namesVersion { 0 };

    std::array<std::atomic<float>, kChannels> channelPeaks {};
    std::array<std::atomic<float>, 2> mainPeaks {}, cuePeaks {};
    std::array<std::atomic<bool>, kChannels> channelSolo {};
    std::array<std::atomic<bool>, mix::kBuses> busSolo {};
    std::atomic<int> soloVersion { 0 };
    juce::RangedAudioParameter *talkParam = nullptr, *oscOn = nullptr;
    std::array<juce::RangedAudioParameter*, kChannels> soloSafeParams {};
    std::array<juce::RangedAudioParameter*, mix::kBuses> busSoloSafeParams {};
    std::array<std::array<std::atomic<float>, 2>, mix::kBuses> busPeaks {};
    std::atomic<float> monoPeak { 0.0f };
    std::array<std::atomic<float>, kChannels> gateReduction {}, compReduction {};
    std::array<std::atomic<float>, mix::kProcessingChains> chainCompReduction {}, chainLimitReduction {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKMixProcessor)
};
