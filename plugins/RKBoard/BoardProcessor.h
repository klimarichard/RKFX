#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MidiLearn.h"
#include "PedalRegistry.h"
#include "Tempo.h"
#include "Routing.h"
#include "dsp/LinkwitzRiley4.h"
#include "plugin/PresetLibrary.h"

#include <array>
#include <atomic>

namespace rkboard
{

/** The RKFX pedalboard: built-in RKFX pedals (up to kMaxPedals, a safety
    cap for the fixed-size audio thread structures, not a design limit),
    placed anywhere along a chain with up to four splits in series (see
    Section); each split sends the signal down two parallel paths that its
    merge adds back.

    Each pedal is the plugin's own processor (same DSP, parameters, presets
    and editor as the standalone plugin), called directly by the board.

    Pedals have an ID that stays with the pedal when it moves (assignments,
    scenes and MIDI learn refer to it). Each section is a list of IDs in
    signal order. A pedal's bypass is the board's: scenes store it, trails
    pedals keep running while bypassed, and an Assign slot can switch it.

    Each split sends both paths the full signal, a 4th-order Linkwitz-Riley
    band split (lows to A, highs to B), or left to A and right to B. Its
    merge adds them back with a level and pan per path and an optional
    polarity flip on B. An empty path passes its signal through; the faster
    path is delayed to line up with the slower one (oversampled drives).
    Every split has its own settings; they are board state (not host
    parameters) but can be assigned and scene-controlled like knobs, under
    the owner number routingOwner (split).

    Threads: the pedal list and chain are changed on the message thread. A
    new pedal is created and prepared there first; the audio thread only ever
    sees a finished chain, swapped in under a short spin lock (no allocation
    or preparation inside it). A removed pedal is deleted after the swap. */
class BoardProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorParameter::Listener,
                       private juce::AsyncUpdater
{
public:
    static constexpr int kMaxPedals = 64;
    static constexpr int kNumAssigns = 32;
    static constexpr int kNumScenes = 16;
    static constexpr int kMaxSceneKnobs = 48;

    BoardProcessor();
    ~BoardProcessor() override;

    //==============================================================================
    // The board (message thread)

    /** Every pedal ID, in section order. */
    std::vector<int> getChain() const;

    /** The pedal IDs of one section, in signal order. */
    const std::vector<int>& getSection (Section s) const { return layout[(size_t) s]; }

    /** The section pedal @p id is in (chain 0 when it isn't on the board). */
    Section sectionOf (int id) const;

    int getNumSplits() const noexcept   { return numSplits; }
    int getNumSections() const noexcept { return 3 * numSplits + 1; }

    /** True while one of split @p split's paths holds a pedal (it takes part). */
    bool isSplitActive (int split = 0) const;

    RoutingSettings& getRouting (int split = 0) noexcept { return splits[(size_t) split]->settings; }

    /** Inserts a split (with empty paths, default settings) into chain
        @p chain before its pedal at @p position; the pedals from there on
        follow the new merge. Returns the new split's index, or -1 when the
        board has kMaxSplits already. */
    int insertSplit (Section chain, int position);

    /** Removes split @p split: its paths' pedals (A, then B) join the chain
        in their place, followed by the chain after its merge. */
    void removeSplit (int split);

    /** The owner number of split @p split's settings, for assignments and
        scene knobs (pedals are 1 and up). */
    static constexpr int routingOwner (int split) noexcept { return -(split + 1); }

    /** A parameter of a pedal (owner >= 1) or of a split's settings
        (routingOwner), or nullptr. */
    juce::RangedAudioParameter* findOwnerParam (int owner, const juce::String& paramId) const;

    /** The pedal with @p id, or nullptr. */
    rkfx::ProcessorBase* getPedal (int id) const;

    /** Adds a pedal of @p typeName to @p section at @p position (-1 = at the
        end). Returns its ID, or 0 when the board is full or the type unknown. */
    int addPedal (const juce::String& typeName, Section section = Section::pre, int position = -1);

    /** Adds a copy of pedal @p id (same values) right after it. Returns the new ID or 0. */
    int duplicatePedal (int id);

    void removePedal (int id);

    /** Moves pedal @p id to @p position in @p section. Within its own section
        the position counts the pedals before the move (so dropping a pedal
        just after itself leaves it where it is). */
    void movePedal (int id, Section section, int position);

    /** Removes every pedal. */
    void clearBoard();

    /** Called on the message thread just before pedal @p id is deleted (the
        editor closes that pedal's editor). */
    std::function<void (int id)> pedalAboutToBeRemoved;

    bool isPedalBypassed (int id) const;
    void setPedalBypassed (int id, bool bypassed);

    /** Bumped whenever the chain or a pedal's preset changes, so the editor
        can cheaply poll for updates. */
    int getBoardCounter() const noexcept { return boardCounter.load(); }

    //==============================================================================
    // In the assignment and scene functions below, "pedalId" is an owner:
    // a pedal ID, or routingOwner (split) for a split's settings.

    //==============================================================================
    // Assignable parameters (message thread): host parameters "Assign 1" to
    // "Assign 32", each driving one knob of one pedal between a minimum and a
    // maximum, optionally inverted (for expression pedals), or a pedal's
    // on/off switch (paramId "bypass"; at 50% and above the pedal is on, the
    // range is ignored). Moving the knob or switch itself moves its Assign
    // parameter too, so a host widget stays in step.

    struct AssignmentInfo
    {
        int pedal = 0;              // 0 = unassigned
        juce::String paramId;
        float min = 0.0f, max = 1.0f;   // normalised range of the target
        bool invert = false;
    };

    AssignmentInfo getAssignment (int slot) const;    // slot 0..31

    /** Assigns @p slot to a pedal's parameter (full range, not inverted). The
        knob keeps its value: the Assign parameter is set to match. A knob has
        at most one slot; assigning it again moves it. */
    void assign (int slot, int pedalId, const juce::String& paramId);
    void clearAssignment (int slot);
    void setAssignmentRange (int slot, float min, float max, bool invert);

    /** The slot driving this pedal parameter, or -1. */
    int findAssignment (int pedalId, const juce::String& paramId) const;

    /** "Delay: Time" or "Delay: On/Off", or empty. */
    juce::String describeAssignment (int slot) const;

    /** True when @p slot switches a pedal on and off. */
    bool isSwitchAssignment (int slot) const;

    juce::AudioParameterFloat& getAssignParam (int slot) const noexcept { return *assignParams[(size_t) slot]; }

    //==============================================================================
    // Scenes: 16 per board. Each stores every pedal's bypass, and the value of
    // every scene-controlled knob (right-click a knob to make it one); other
    // knobs are the same in all scenes. Recalling a scene switches bypasses at
    // once (delay and reverb tails ring out with trails on) and glides the
    // scene-controlled knobs. Edits while a scene is active go into it.
    // Recalled through the host parameter "Scene" (1-16), the scene buttons,
    // and MIDI program changes 0-15 in the standalone app.

    int getScene() const noexcept { return currentScene.load(); }     // 0..15
    void setScene (int index);
    juce::RangedAudioParameter& getSceneParam() const noexcept { return *sceneParam; }

    bool isSceneControlled (int pedalId, const juce::String& paramId) const;
    /** Makes a knob scene-controlled: every scene starts at its current value.
        Returns false when the board's 48 scene knobs are taken. */
    bool setSceneControlled (int pedalId, const juce::String& paramId, bool controlled);

    bool getSceneBypass (int scene, int pedalId) const;
    float getSceneValue (int scene, int pedalId, const juce::String& paramId) const;   // normalised
    void copyScene (int from, int to);

    //==============================================================================
    // Trails (delay and reverb pedals): when bypassed with trails on, the pedal
    // stops taking input and its echoes and tail ring out.

    bool canHaveTrails (int pedalId) const;
    bool getTrails (int pedalId) const;
    void setTrails (int pedalId, bool on);

    //==============================================================================
    // Standalone only (in a host, Gig Performer does these): tempo with tap
    // and MIDI clock, MIDI learn, and the tuner.

    bool isStandalone() const noexcept { return standalone; }

    /** Tests: behave as the standalone app, with the rig's files in @p rigFolder. */
    void setStandaloneForTesting (const juce::File& rigFolder);
    /** Tests: runs a pending update now (what the message loop would do). */
    void flushPendingUpdate() { handleUpdateNowIfNeeded(); }

    /** The board's tempo (stored in board presets; the pedals' tempo in the
        standalone app). Following MIDI clock replaces it. */
    double getTempo() const noexcept { return tempo.load(); }
    void setTempo (double bpm);
    void tapTempo();

    bool isFollowingClock() const noexcept { return rig.followClock; }
    void setFollowClock (bool follow);

    MidiMap* getMidiMap() noexcept { return midiMap.get(); }
    /** The next MIDI controller moved is learned for @p target (a LearnTarget). */
    void startLearning (int target) { learning = target; }
    void cancelLearning() { learning = -1; }
    int getLearningTarget() const noexcept { return learning.load(); }

    bool isTunerActive() const noexcept { return tunerActive.load(); }
    void setTunerActive (bool on) { tunerActive = on; ++boardCounter; }
    float getTunerReference() const noexcept { return rig.tunerReference; }
    bool getTunerMutes() const noexcept { return rig.tunerMutes; }
    void setTunerOptions (float reference, bool mutes);

    /** The latest @p numSamples of input (mono) for the tuner; message thread. */
    void copyTunerInput (float* dest, int numSamples) const;
    static constexpr int kTunerBufferSize = 8192;

    juce::RangedAudioParameter& getInputParam() const noexcept  { return *inputParam; }
    juce::RangedAudioParameter& getOutputParam() const noexcept { return *outputParam; }

    //==============================================================================
    // Board presets (message thread), same JSON conventions as plugin presets:
    //     { "plugin": "RKBoard", "version": 1, "name": ...,
    //       "board": { "input": 0, "output": 0, "tempo": 120 },
    //       "splits": [ { "split": "frequency", "crossover": 800, ... }, ... one per split ],   (older boards: one, "routing": { ... })
    //       "assign": [ { "slot": 1, "pedal": 3, "param": "time", "min": 100, "max": 800, "invert": false } ],
    //       "scenes": { "current": 1, "bypass": [ [2, 5], [], ... one list of bypassed IDs per scene ],
    //                   "knobs": [ { "pedal": 3, "param": "time", "values": [ 350, 500, ... 16 ] } ] },
    //       "pedals": [ { "id": 3, "section": "a", "bypass": false,
    //                     "preset": { "plugin": "RKChorus", "version": 1, "params": {...} } }, ... ] }
    // A pedal's "preset" is exactly a plugin preset, so one can be copied out
    // into a plugin's preset folder and back.
    struct PresetResult
    {
        bool ok = false;
        juce::String message;
    };

    PresetResult loadBoardPreset (const juce::File&);

    /** The built-in board presets (read-only, like the plugins' factory presets). */
    const std::vector<rkfx::FactoryPreset>& getFactoryPresets() const noexcept { return factoryPresets; }
    PresetResult loadFactoryPreset (int index);
    PresetResult saveBoardPreset (const juce::File&);
    void resetBoard();

    juce::String getPresetName() const;
    juce::File getPresetFile() const;
    bool isPresetModified() const noexcept { return presetModified.load(); }
    int getPresetStateCounter() const noexcept { return presetStateCounter.load(); }

    rkfx::PresetLibrary& getLibrary() noexcept { return library; }

    /** Board as JSON: every pedal's full state for the host (@p fullState),
        or only non-default values for a preset file. */
    juce::var toVar (bool fullState) const;
    PresetResult fromVar (const juce::var&, bool fullState);

    //==============================================================================
    // juce::AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RKBoard"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    struct SectionChain
    {
        std::array<rkfx::ProcessorBase*, kMaxPedals> pedals {};
        std::array<int, kMaxPedals> ids {};
        int size = 0;
    };

    /** What the audio thread runs: every section, and each split's settings. */
    struct Chain
    {
        std::array<SectionChain, kMaxSections> sections {};
        std::array<RoutingSettings*, kMaxSplits> routing {};
        int numSplits = 1;
    };

    /** A short delay (whole samples) lining up the faster path with the slower one. */
    struct PathDelay
    {
        void prepare (int numChannels) { ring.setSize (numChannels, kSize); ring.clear(); write = 0; }
        void process (juce::AudioBuffer<float>& buffer, int numSamples, int delay) noexcept;
        static constexpr int kSize = 1024;   // > kMaxPedals x the most latency one pedal reports
        juce::AudioBuffer<float> ring;
        int write = 0;
    };

    rkfx::PresetLibrary library;
    std::vector<rkfx::FactoryPreset> factoryPresets { (rkboard::registerFactoryPresets(), rkfx::factoryPresetsFor ("RKBoard")) };
    // Pedals by ID - 1. Written on the message thread only; the audio thread
    // reads them through `chain`, under chainLock.
    std::array<std::unique_ptr<rkfx::ProcessorBase>, kMaxPedals> pedals;
    std::array<std::vector<int>, kMaxSections> layout;   // message thread copy, as IDs
    int numSplits = 1;

    juce::SpinLock chainLock;
    Chain chain;                             // audio thread view (under chainLock)

    // Splits and merges (audio thread): the paths' buffers are shared, the
    // rest is per split.
    juce::AudioBuffer<float> pathBuffers[2];
    struct SplitDsp
    {
        PathDelay delays[2];
        std::array<rkfx::LinkwitzRiley4, 2> crossovers;     // per channel
        juce::SmoothedValue<float> mergeGains[2][2];       // [path][channel], pan and level and polarity folded in
        bool wasActive = false;
    };
    std::array<SplitDsp, kMaxSplits> splitDsp;

    // Each pedal's bypass, as the board sees it (a trails pedal's own bypass
    // stays off while it rings out). Any thread.
    std::array<std::atomic<bool>, kMaxPedals> bypassState {};

    /** One slot, readable from any thread. */
    struct Assignment
    {
        std::atomic<int> pedal { 0 };
        std::atomic<int> paramIndex { -1 };    // in the pedal's getParameters()
        std::atomic<float> min { 0.0f }, max { 1.0f };
        std::atomic<bool> invert { false };
    };
    std::array<Assignment, kNumAssigns> assignments;
    std::array<juce::AudioParameterFloat*, kNumAssigns> assignParams {};
    std::atomic<bool> syncingAssign { false };

    // Scenes
    juce::AudioParameterInt* sceneParam = nullptr;
    std::atomic<int> currentScene { 0 };
    std::array<std::array<std::atomic<bool>, kMaxPedals>, kNumScenes> sceneBypass {};

    struct SceneKnob
    {
        std::atomic<int> pedal { 0 };          // 0 = free
        std::atomic<int> paramIndex { -1 };
        std::array<std::atomic<float>, kNumScenes> values {};
        std::atomic<float> glideTarget { 0.0f };
        std::atomic<bool> gliding { false };
    };
    std::array<SceneKnob, kMaxSceneKnobs> sceneKnobs;
    static inline thread_local bool applyingScene = false;    // this thread is recalling a scene

    void recallScene (int index);
    void glideSceneKnobs (int numSamples);
    void clearSceneKnobsOf (int pedalId);
    int findSceneKnob (int pedalId, int paramIndex) const;

    // Trails
    std::array<std::atomic<bool>, kMaxPedals> trails {};
    std::array<std::atomic<bool>, kMaxPedals> trailBypassed {};
    std::array<bool, kMaxPedals> trailIdle {};              // audio thread
    std::array<int, kMaxPedals> trailSilence {};            // audio thread, samples
    juce::AudioBuffer<float> trailBuffer;

    // Standalone
    bool standalone = false;
    std::unique_ptr<MidiMap> midiMap;
    RigSettings rig;
    juce::File rigSettingsFile;
    std::atomic<double> tempo { 120.0 };
    BoardPlayHead boardPlayHead;
    TapTempo tapper;
    MidiClockFollower clock;
    double sampleTime = 0.0;                       // seconds of audio processed (MIDI timing)
    std::atomic<int> learning { -1 };
    std::atomic<int> learnedTarget { -1 }, learnedChannel { 0 }, learnedCC { 0 };
    std::array<std::atomic<float>, kNumAssigns> ccTargets {};
    std::array<std::atomic<bool>, kNumAssigns> ccGliding {};
    std::array<bool, 256> switchHeld {};           // last state of each switch target (press edges)
    std::atomic<bool> tunerActive { false };
    juce::AudioBuffer<float> tunerRing;
    std::atomic<int> tunerWrite { 0 };
    juce::SmoothedValue<float> tunerMute { 1.0f };

    void handleStandaloneMidi (const juce::MidiBuffer&, int numSamples);
    void applyController (int target, bool momentary, int value, double time);
    void glideControllers (int numSamples);
    static inline thread_local bool applyingAssign = false;   // this thread is moving a knob for an Assign parameter

    juce::RangedAudioParameter* assignTarget (int slot) const;      // under chainLock on the audio thread
    void applyAssign (int slot, float value);                       // Assign parameter -> knob
    void followTarget (int pedalId, int paramIndex, float value);   // knob or switch -> Assign parameter
    bool isBypassIndex (int pedalId, int paramIndex) const;
    float targetValue (int pedalId, int paramIndex) const;          // normalised; a switch reads its bypass
    void clearAssignmentsOf (int pedalId);
    juce::AudioParameterFloat* inputParam = nullptr;
    juce::AudioParameterFloat* outputParam = nullptr;
    std::atomic<bool> syncingBypass { false };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain { 1.0f }, outputGain { 1.0f };
    juce::MidiBuffer pedalMidi;
    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;
    std::atomic<int> totalLatency { 0 };
    std::atomic<double> tailSeconds { 0.0 };

    // Board preset state
    mutable juce::CriticalSection presetLock;
    juce::String presetName { "Default" };
    juce::File presetFile;
    std::atomic<bool> presetModified { false };
    std::atomic<int> presetStateCounter { 0 };
    std::atomic<int> boardCounter { 0 };
    std::atomic<bool> loadingBoard { false };

    /** Listens to every parameter of one pedal: its own bypass switch (in its
        editor) bypasses it on the board, any other change marks the board
        preset modified. */
    struct PedalLink : public juce::AudioProcessorParameter::Listener
    {
        PedalLink (BoardProcessor& b, int pedalId) : board (b), id (pedalId) {}
        void parameterValueChanged (int parameterIndex, float newValue) override;
        void parameterGestureChanged (int, bool) override {}
        BoardProcessor& board;
        const int id;
    };
    std::array<std::unique_ptr<PedalLink>, kMaxPedals> links;

    void attachPedal (int id, std::unique_ptr<rkfx::ProcessorBase>);
    std::unique_ptr<rkfx::ProcessorBase> detachPedal (int id);
    void markModified();

    std::unique_ptr<rkfx::ProcessorBase> createPedal (const juce::String& typeName) const;
    void preparePedal (rkfx::ProcessorBase&) const;
    int freeId() const;
    void publishChain();
    void boardChanged();
    void setPresetState (const juce::String& name, const juce::File& file, bool modified);
    void updateLatency();
    int sectionLatency (const SectionChain&) const;
    void processSection (const SectionChain&, juce::AudioBuffer<float>&, const juce::MidiBuffer&, int midiOffset);
    void setPedalOwnBypass (int id, bool bypassed);
    int paramIndexOf (int pedalId, const juce::String& paramId) const;
    void processChunk (juce::AudioBuffer<float>&, const juce::MidiBuffer&, int midiOffset);
    void processSplit (int split, juce::AudioBuffer<float>& main, const juce::MidiBuffer&, int midiOffset);
    void updateMergeTargets (int split, bool snap);

    struct Split;

    /** Any routing change marks the board modified (and follows assignments
        and scenes, like a knob); choosing the stereo split pans the paths
        left and right (and back to the centre when leaving it). */
    struct RoutingWatcher : public juce::AudioProcessorParameter::Listener
    {
        RoutingWatcher (BoardProcessor& b, Split& s) : board (b), split (s) {}
        void parameterValueChanged (int parameterIndex, float newValue) override;
        void parameterGestureChanged (int, bool) override {}
        BoardProcessor& board;
        Split& split;
    };

    /** One split's settings. Splits move as others are inserted or removed
        (index follows), so each keeps its own settings. */
    struct Split
    {
        Split (BoardProcessor& b, int i) : watcher (b, *this), index (i) {}
        RoutingSettings settings;
        RoutingWatcher watcher;
        SplitMode lastMode = SplitMode::full;
        std::atomic<int> index;
    };
    std::array<std::unique_ptr<Split>, kMaxSplits> splits;

    juce::AudioProcessor* ownerProcessor (int owner) const;
    void remapOwner (int from, int to);          // assignments and scene knobs follow a moved split
    void resetSplitSettings (Split&);

    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override;

    friend class BoardEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardProcessor)
};

} // namespace rkboard
