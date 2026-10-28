#pragma once

#include "plugin/EditorBase.h"

class RKConductProcessor;

class RKConductEditor : public rkfx::EditorBase
{
public:
    explicit RKConductEditor (RKConductProcessor&);

private:
    RKConductProcessor& conductor;

    /** A button held like a footswitch: the processor acts on the press. */
    struct Footswitch : public juce::TextButton
    {
        Footswitch (juce::RangedAudioParameter&, const juce::String& text);
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        juce::RangedAudioParameter& param;
    };

    /** One track: name, notes, on/off and output channel. */
    struct TrackRow : public juce::Component
    {
        TrackRow (RKConductProcessor&, int track);
        void resized() override;
        void paint (juce::Graphics&) override;
        RKConductProcessor& processor;
        const int track;
        juce::ToggleButton on;
        juce::ComboBox channel;
    };

    juce::TextButton loadButton { "Load MIDI file" };
    juce::ComboBox beatsPerBar, beatValue;
    std::unique_ptr<juce::ComboBoxParameterAttachment> beatsAttachment, valueAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String note;
    std::vector<std::unique_ptr<TrackRow>> rows;
    int shownVersion = -1;

    std::unique_ptr<Footswitch> tap, stop;
    std::unique_ptr<rkfx::Knob> tapEvery, tempo, countIn, key1, key2, tapCC;
    std::unique_ptr<rkfx::ToggleSwitch> fileTempo, loop;
    std::unique_ptr<juce::ParameterAttachment> tempoWatcher;

    juce::Rectangle<int> songArea() const;
    juce::Rectangle<int> conductArea() const;
    juce::Rectangle<int> statusArea() const;
    void rebuildRows();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    void refresh();
    juce::TimedCallback refresher { [this] { refresh(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKConductEditor)
};
