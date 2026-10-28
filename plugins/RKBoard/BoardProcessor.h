#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PedalRegistry.h"
#include "plugin/PresetLibrary.h"

#include <array>
#include <atomic>

namespace rkboard
{

/** The RKFX pedalboard: up to 12 built-in RKFX pedals in a serial chain.

    Each pedal is the plugin's own processor (same DSP, parameters, presets
    and editor as the standalone plugin), called directly by the board.

    Pedals have an ID from 1 to 12 that stays with the pedal when it moves;
    the host parameter "Pedal N Bypass" belongs to pedal ID N, so a mapping
    follows its pedal. The chain order is a list of IDs.

    Threads: the pedal list and chain are changed on the message thread. A
    new pedal is created and prepared there first; the audio thread only ever
    sees a finished chain, swapped in under a short spin lock (no allocation
    or preparation inside it). A removed pedal is deleted after the swap. */
class BoardProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorParameter::Listener,
                       private juce::AsyncUpdater
{
public:
    static constexpr int kMaxPedals = 12;

    BoardProcessor();
    ~BoardProcessor() override;

    //==============================================================================
    // The board (message thread)

    /** Pedal IDs in signal order. */
    std::vector<int> getChain() const;

    /** The pedal with @p id, or nullptr. */
    rkfx::ProcessorBase* getPedal (int id) const;

    /** Adds a pedal of @p typeName at chain position @p position (-1 = at the
        end). Returns its ID, or 0 when the board is full or the type unknown. */
    int addPedal (const juce::String& typeName, int position = -1);

    /** Adds a copy of pedal @p id (same values) right after it. Returns the new ID or 0. */
    int duplicatePedal (int id);

    void removePedal (int id);

    /** Moves pedal @p id to chain position @p position. */
    void movePedal (int id, int position);

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

    juce::RangedAudioParameter& getInputParam() const noexcept  { return *inputParam; }
    juce::RangedAudioParameter& getOutputParam() const noexcept { return *outputParam; }

    //==============================================================================
    // Board presets (message thread), same JSON conventions as plugin presets:
    //     { "plugin": "RKBoard", "version": 1, "name": ...,
    //       "board": { "input": 0, "output": 0 },
    //       "pedals": [ { "id": 3, "bypass": false,
    //                     "preset": { "plugin": "RKChorus", "version": 1, "params": {...} } }, ... ] }
    // A pedal's "preset" is exactly a plugin preset, so one can be copied out
    // into a plugin's preset folder and back.
    struct PresetResult
    {
        bool ok = false;
        juce::String message;
    };

    PresetResult loadBoardPreset (const juce::File&);
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
    struct Chain
    {
        std::array<rkfx::ProcessorBase*, kMaxPedals> pedals {};
        int size = 0;
    };

    rkfx::PresetLibrary library;

    // Pedals by ID - 1. Written on the message thread only; the audio thread
    // reads them through `chain`, under chainLock.
    std::array<std::unique_ptr<rkfx::ProcessorBase>, kMaxPedals> pedals;
    std::vector<int> order;                  // message thread copy of the chain, as IDs

    juce::SpinLock chainLock;
    Chain chain;                             // audio thread view (under chainLock)

    std::array<juce::AudioParameterBool*, kMaxPedals> bypassParams {};
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

    /** Listens to every parameter of one pedal: its bypass is mirrored to the
        board's "Pedal N Bypass" parameter, any other change marks the board
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

    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override;

    friend class BoardEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardProcessor)
};

} // namespace rkboard
