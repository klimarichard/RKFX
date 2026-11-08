#pragma once

#include "plugin/ProcessorBase.h"

#include "MixEngine.h"
#include "MixAssigns.h"
#include "MixInserts.h"
#include "MixMonitorEqSession.h"
#include "MixRecorderSession.h"
#include "MixPlayback.h"
#include "MixExport.h"
#include "MixScenes.h"
#include "MixUndo.h"
#include "MixRemote.h"
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

    /** DCA and mute group names, DCA colours (message thread). Saved with the mix. */
    juce::String getDcaName (int dca) const;
    void setDcaName (int dca, const juce::String&);
    int getDcaColour (int dca) const noexcept { return dcaColours[(size_t) dca].load(); }
    void setDcaColour (int dca, int colour);
    juce::String getMuteGroupName (int group) const;
    void setMuteGroupName (int group, const juce::String&);

    /** The members of a DCA or a mute group, as the user pages list strips:
        a channel (a linked pair's left channel) or kUserPageBus + a bus. */
    juce::Array<int> getDcaMembers (int dca) const;
    juce::Array<int> getMuteGroupMembers (int group) const;

    /** True while a DCA's mute or an active mute group silences the strip
        (its own Mute may be off). A linked pair: its left channel. */
    bool isChannelGroupMuted (int ch) const;
    bool isBusGroupMuted (int bus) const;

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
    /** Monitor EQ: the measuring (engine) and its flow (message thread). */
    MixMeasure& getMeasure() noexcept { return engine.getMeasure(); }
    MixMonitorEqSession& getMonitorEq() noexcept { return *monitorEq; }
    MixRecorderSession& getRecorder() noexcept { return *recorder; }

    /** Virtual soundcheck: a recording played into the inputs set to Playback.
        The source of each input is never saved, never in scenes, never undone:
        every session and every loaded mix starts all Live. A linked pair's
        two inputs switch together. */
    MixPlayback& getPlayback() noexcept { return playback; }
    void setInputOnPlayback (int input, bool onPlayback);
    bool isInputOnPlayback (int input) const noexcept { return inputOnPlayback[(size_t) input].load(); }
    void setAllInputsLive();
    void setAllInputsOnPlayback();
    bool anyInputOnPlayback() const noexcept;
    /** Bumped when an input's source changes (the window polls it). */
    int getSourceVersion() const noexcept { return sourceVersion.load(); }

    /** Export: the main mix of a region of the loaded recording, faster than real time. */
    MixExport& getExport() noexcept { return exporter; }
    /** The mix as it is now, for an export (message thread): its settings, the
        faders and mutes (no solo, talkback or oscillator), a copy of every insert, the tempo. */
    MixExport::Mix exportMix();

    /** The monitor remote (message thread). */
    MixRemote& getRemote() noexcept { return *remote; }
    /** The remote's meters, apart from the window's, since the last call (it resets them). */
    using RemoteMeters = MixRemoteMeters;
    void takeRemoteMeters (RemoteMeters&) noexcept;

    /** The console's undo history (message thread). */
    MixUndo& getUndo() noexcept { return *undo; }

    /** Show lock (the window): Off, faders and mutes only, or everything. Not saved. */
    enum class Lock { off = 0, faders, everything };
    Lock getLock() const noexcept { return (Lock) lock.load(); }
    void setLock (Lock l) noexcept { lock = (int) l; }
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

    /** Copy and paste of a strip's section (message thread). A channel's
        sections are those of its page; a bus's, the main's and the mono
        sum's (a chain) are its EQ, compressor and limiter. "Whole" is every
        section and the inserts (not routing, names or colours). The copy is
        a var that names its kind and section ("Gate from Lead Vocal"). */
    enum class Section { input, delay, hpf, gate, eq, comp, limit, whole };
    static juce::String sectionName (Section);
    juce::var copyChannel (int channel, Section) const;
    juce::var copyChain (int chain, Section) const;
    /** True when @p copy fits a channel (or a chain); then pasting it is allowed. */
    static bool isChannelCopy (const juce::var& copy);
    static bool isChainCopy (const juce::var& copy);
    void pasteChannel (int channel, const juce::var& copy);
    void pasteChain (int chain, const juce::var& copy);
    /** "Gate from Lead Vocal", "RKVerb from Reverb", "Send EQ from Lead Vocal". */
    static juce::String describeCopy (const juce::var& copy);

    /** A send's EQ (docs/design/RKMix Send EQ.md): channel @p channel's send
        to Aux or FX bus @p bus (a linked pair: its left channel's). */
    SendEqDsp::Settings getSendEq (int channel, int bus) const;
    /** Its settings, in the order of mix::sendEqFields (the remote reads and sets them). */
    const std::array<juce::RangedAudioParameter*, mix::kSendEqFields>& getSendEqParams (int channel, int bus) const
    {
        return channelRouting[(size_t) channel].sendEqs[(size_t) bus];
    }
    /** True when channel @p channel's send to @p bus can have an EQ (an Aux or FX bus). */
    bool hasSendEq (int bus) const;
    juce::var copySendEq (int channel, int bus) const;
    static bool isSendEqCopy (const juce::var& copy);
    /** Pastes onto one send (the window adds the ganged channels). */
    void pasteSendEq (int channel, int bus, const juce::var& copy);
    /** Pastes onto every channel's send to @p bus (each linked pair once). */
    void pasteSendEqToBus (int bus, const juce::var& copy);

    /** One insert with its settings and on/off (nothing when the slot is empty). */
    juce::var copyInsert (InsertRack::Slot) const;
    static bool isInsertCopy (const juce::var& copy);
    /** Puts a copied insert in @p slot of any strip (replacing what was there), mono or stereo as the strip. */
    bool pasteInsert (InsertRack::Slot, const juce::var& copy);

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
        std::array<juce::RangedAudioParameter*, mix::kBuses> sends {}, taps {}, assigns {}, sendPans {};
        std::array<std::array<juce::RangedAudioParameter*, mix::kSendEqFields>, mix::kBuses> sendEqs {};   // channels only
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

    // DCAs and mute groups: their host parameters, and each strip's membership.
    std::array<juce::RangedAudioParameter*, mix::kDcas> dcaFaders {}, dcaMutes {};
    std::array<juce::RangedAudioParameter*, mix::kMuteGroups> muteGroups {};
    struct Membership
    {
        std::array<juce::RangedAudioParameter*, mix::kDcas> dcas {};
        std::array<juce::RangedAudioParameter*, mix::kMuteGroups> muteGroups {};
    };
    std::array<Membership, kChannels> channelMembership {};
    std::array<Membership, mix::kBuses> busMembership {};

    /** A strip's fader with its DCAs (dB), and whether a DCA or a mute group mutes it. */
    float withDcas (float faderDb, const Membership&, const std::array<float, mix::kDcas>& dcaDb) const;
    bool groupMuted (const Membership&) const;

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
    std::unique_ptr<MixUndo> undo;
    std::unique_ptr<MixRemote> remote;
    std::unique_ptr<MixMonitorEqSession> monitorEq;
    std::unique_ptr<MixRecorderSession> recorder;
    MixPlayback playback;
    MixExport exporter;
    std::array<std::atomic<bool>, kChannels> inputOnPlayback {};
    std::atomic<int> sourceVersion { 0 };
    juce::AudioBuffer<float> playbackInputs;            // the inputs with the recording in place of the live ones
    double preparedRate = 0.0;

    /** The Record and Marker host parameters (any thread): to the recorder. */
    struct RecordSync : public juce::AudioProcessorParameter::Listener
    {
        explicit RecordSync (RKMixProcessor& p) : owner (p) {}
        void parameterValueChanged (int index, float value) override;
        void parameterGestureChanged (int, bool) override {}
        RKMixProcessor& owner;
        std::atomic<bool> markerDown { false };
    };
    RecordSync recordSync { *this };
    juce::RangedAudioParameter *recordParam = nullptr, *markerParam = nullptr;
    std::array<std::atomic<float>, kChannels> remoteChannelPeaks {};
    std::array<std::array<std::atomic<float>, 2>, mix::kBuses> remoteBusPeaks {};
    std::array<std::atomic<float>, 2> remoteMainPeaks {};
    std::atomic<float> remoteMonoPeak { 0.0f };
    std::array<std::atomic<float>, kChannels> remoteGate {}, remoteComp {};
    std::array<std::atomic<float>, mix::kProcessingChains> remoteChainComp {}, remoteChainLimit {};
    std::atomic<int> lock { 0 };
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

    void readSettings (std::uint64_t parts) { readSettings (engineSettings, parts); }   // mix settings -> engineSettings
    void readSettings (MixEngine::Settings&, std::uint64_t parts) const;
    std::atomic<double> hostBpm { 120.0 };               // the host's tempo, last seen (for an export)
    static bool isStripField (int field);               // in channel presets (not routing)
    std::vector<rkfx::ParamInfo> stripInfos() const;
    MixEngine::Levels readLevels() const;

    mutable juce::CriticalSection namesLock;
    std::array<juce::String, kChannels> channelNames, outputNames;
    std::array<juce::String, mix::kBuses> busNames;
    std::array<juce::String, mix::kDcas> dcaNames;
    std::array<juce::String, mix::kMuteGroups> muteGroupNames;
    std::array<std::atomic<int>, mix::kDcas> dcaColours {};
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
