#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/PresetFormat.h"

namespace rkboard
{

/** Where a pedal sits. A board is a chain with up to kMaxSplits splits in
    series; each split sends the signal down two paths, and its merge adds
    them back:

        In -> [chain 0] -> split 1 -+- [1A] -+- merge 1 -> [chain 1] -> split 2 ... -> [chain n] -> Out
                                    +- [1B] -+

    Sections are numbered in signal order: chain k is 3k, and split k's
    paths (k from 0) are 3k + 1 (A) and 3k + 2 (B). So pre, pathA, pathB
    and post name chain 0, the first split's paths and chain 1, as on a
    board with one split. A split only takes part while one of its paths
    holds a pedal; with both empty the signal passes it untouched. */
enum class Section : int { pre = 0, pathA = 1, pathB = 2, post = 3 };

constexpr int kMaxSplits = 4;
constexpr int kMaxSections = 3 * kMaxSplits + 1;

constexpr Section chainSection (int k) noexcept           { return (Section) (3 * k); }
constexpr Section pathSection (int split, int path) noexcept { return (Section) (3 * split + 1 + path); }
constexpr bool isPath (Section s) noexcept               { return (int) s % 3 != 0; }
constexpr int splitOf (Section s) noexcept               { return (int) s / 3; }       // a path's split
constexpr int pathOf (Section s) noexcept                { return (int) s % 3 - 1; }   // 0 = A, 1 = B
constexpr int chainOf (Section s) noexcept               { return (int) s / 3; }       // a chain's number

juce::String sectionKey (Section);                       // "chain0", "a1", "b1", "chain1", ...
Section sectionFromKey (const juce::String&);            // also the older "pre", "a", "b", "post"; unknown -> chain 0

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
