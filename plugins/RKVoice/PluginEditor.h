#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "VoiceEngine.h"

class RKVoiceProcessor;

/** RKVoice's window: Harmony (mode, chord settings, key and scale), Input
    (range, threshold, dry), a line showing what's happening (the sung note,
    the chord in use, each voice's note), and the four voices: position in a
    chord and interval in the scale side by side (the one not in use dimmed),
    level and pan. Detune, Humanize and Glide sit in the bottom row. */
class RKVoiceEditor : public rkfx::EditorBase
{
public:
    explicit RKVoiceEditor (RKVoiceProcessor&);

private:
    RKVoiceProcessor& voice;
    rkfx::ParameterWatcher watcher;

    std::unique_ptr<rkfx::SegmentedSwitch> mode, range;
    std::unique_ptr<rkfx::Knob> chordTones, noChord, midiChannel, key, scale, threshold, dry, detune, humanize, glide;
    struct VoiceKnobs { std::unique_ptr<rkfx::Knob> position, interval, level, pan; };
    std::array<VoiceKnobs, VoiceEngine::kNumVoices> voices;

    juce::TimedCallback refresher { [this] { repaint(); } };

    juce::Rectangle<int> harmonyArea() const;
    juce::Rectangle<int> inputArea() const;
    juce::Rectangle<int> statusArea() const;
    juce::Rectangle<int> voicesArea() const;
    juce::Rectangle<int> voiceColumn (int v) const;

    void onParameterChange();
    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKVoiceEditor)
};
