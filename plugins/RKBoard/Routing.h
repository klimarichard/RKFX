#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/PresetFormat.h"

namespace rkboard
{

/** Where a pedal sits:

        Input -> [pre] -> split -+- [path A] -+- merge -> [post] -> Output
                                 +- [path B] -+

    The split and merge only take part while path A or B holds a pedal; with
    both empty the board is a plain serial chain (pre, then post). */
enum class Section { pre, pathA, pathB, post };
constexpr int kNumSections = 4;

juce::String sectionKey (Section);                       // "pre", "a", "b", "post"
Section sectionFromKey (const juce::String&);            // unknown -> pre

enum class SplitMode { full, frequency, stereo };

/** The split and merge settings. They are board state (saved with the board,
    not host parameters), held as parameters of a processor that never
    processes, so the suite's knobs and switches work with them unchanged.
    The audio thread reads them like any parameter (atomic). */
class RoutingSettings : public juce::AudioProcessor
{
public:
    RoutingSettings();

    juce::RangedAudioParameter& param (const juce::String& id) const;
    float value (const juce::String& id) const;

    SplitMode getSplitMode() const;

    /** The "params" object of a preset: only non-default values for a board
        preset file, every value for the host state. */
    juce::var toVar (bool onlyNonDefault) const;
    /** Reads a "params" object leniently (bad values skipped, missing ones at
        their default). Returns notes about skipped values. */
    juce::StringArray fromVar (const juce::var& params);

    // juce::AudioProcessor (never used for audio)
    const juce::String getName() const override { return "RKBoard routing"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    std::vector<rkfx::ParamInfo> infos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingSettings)
};

} // namespace rkboard
