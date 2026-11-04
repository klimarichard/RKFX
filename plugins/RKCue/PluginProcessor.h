#pragma once

#include "plugin/ProcessorBase.h"

#include "CueEngine.h"
#include "Parameters.h"

/** RKCue: a sampler whose samples are MIDI files (MIDI effect). */
class RKCueProcessor : public rkfx::ProcessorBase
{
public:
    static constexpr int kSlots = CueEngine::kSlots;

    RKCueProcessor();
    ~RKCueProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    /** What a slot holds (message thread). */
    struct SlotFile
    {
        juce::MemoryBlock data;                 // the file itself
        juce::String name;                      // shown on the pad (the file's name, or renamed)
        std::vector<rkfx::MidiSong::TrackInfo> tracks;
        double lengthBeats = 0.0, lengthSeconds = 0.0, tempoBpm = 120.0;
        bool hasFile() const noexcept { return ! data.isEmpty(); }
    };

    // Message thread.
    /** Loads a MIDI file into a slot; returns an error, or empty. */
    juce::String loadFile (int slot, const juce::File&);
    void clearSlot (int slot);
    void setSlotName (int slot, const juce::String& name);
    const SlotFile& getSlot (int slot) const noexcept { return files[(size_t) slot]; }
    /** Bumped whenever a slot's file or name changes. */
    int getFilesVersion() const noexcept { return filesVersion.load(); }

    const CueEngine& getEngine() const noexcept { return engine; }

protected:
    void prepareDsp (double sampleRate, int, int) override { engine.prepare (sampleRate); }
    void resetDsp() override                               { engine.stopAll(); }
    void processDsp (juce::AudioBuffer<float>&) override {}
    void processMidi (juce::MidiBuffer&, int numSamples) override;
    void releaseMidi (juce::MidiBuffer& midi) override { engine.releaseAll (midi); }

    juce::var getExtraState() const override;
    void setExtraState (const juce::var&) override;
    juce::var writePresetFiles (const juce::File& folder) const override;
    juce::String readPresetFiles (const juce::var& extra, const juce::File& folder) override;

private:
    struct SlotParams
    {
        juce::RangedAudioParameter *low, *high, *root, *pitch, *transpose, *drums, *mode, *loop, *poly, *velocity, *channel;
    };

    juce::RangedAudioParameter *tempoSource, *tempo, *thru, *trigChannel, *stopAll;
    std::array<SlotParams, kSlots> slotParams;
    std::array<juce::RangedAudioParameter*, kSlots> playButtons, stopButtons;
    CueEngine engine;

    std::array<SlotFile, kSlots> files;
    std::atomic<int> filesVersion { 0 };

    juce::String useFile (int slot, const juce::MemoryBlock&, const juce::String& name);
    void clearAll();
    static juce::String presetFileName (int slot, const juce::String& name);

    /** Play, Stop and Stop All: each press acts once; Play's release is a Gate slot's key-up. */
    struct ButtonListener : public juce::AudioProcessorParameter::Listener
    {
        explicit ButtonListener (RKCueProcessor& p) : owner (p) {}
        void parameterValueChanged (int index, float value) override;
        void parameterGestureChanged (int, bool) override {}
        RKCueProcessor& owner;
        std::array<std::atomic<bool>, 2 * kSlots + 1> down {};
    };
    ButtonListener buttonListener { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKCueProcessor)
};
