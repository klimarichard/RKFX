#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "RemapEngine.h"

#include <array>

class RKRemapProcessor;

class RKRemapEditor : public rkfx::EditorBase
{
public:
    explicit RKRemapEditor (RKRemapProcessor&);
    ~RKRemapEditor() override;

private:
    /** A signal light: off, present (above -60 dBFS), strong (above -20 dBFS). */
    class Light : public juce::Component
    {
    public:
        explicit Light (juce::Colour accentColour) : accent (accentColour) { setInterceptsMouseClicks (false, false); }
        void setPeak (float peak);
        void paint (juce::Graphics&) override;

    private:
        juce::Colour accent;
        int level = 0;
    };

    /** One input: number, name (double-click to edit), light; the pair's Link on its first row. */
    struct InputRow : public juce::Component
    {
        InputRow (RKRemapEditor&, int channel);
        void resized() override;
        void paint (juce::Graphics&) override;

        RKRemapEditor& owner;
        const int channel;
        juce::Label name;
        Light light;
        std::unique_ptr<rkfx::ToggleSwitch> link;   // first row of a pair only
    };

    /** One output: number, name, Source, Trim, Mute, Polarity, Mono, light, Link. */
    struct OutputRow : public juce::Component
    {
        OutputRow (RKRemapEditor&, int channel);
        void resized() override;
        void paint (juce::Graphics&) override;
        void refreshSources();
        void updateLinked();

        RKRemapEditor& owner;
        const int channel;
        juce::Label name;
        juce::ComboBox source;
        juce::Slider trim;
        std::unique_ptr<juce::SliderParameterAttachment> trimAttachment;
        std::unique_ptr<juce::ParameterAttachment> sourceAttachment;
        std::unique_ptr<rkfx::ToggleSwitch> mute, polarity, mono, link;
        juce::Label monoNote;                       // a single output summing a pair: always mono
        Light light;
        bool follower = false;                      // the right output of a linked pair
    };

    /** The rows of a list, in a viewport. */
    struct List : public juce::Component
    {
        void resized() override;
        std::vector<juce::Component*> rows;
        int shown = 16, rowHeight = 30;
    };

    RKRemapProcessor& remap;
    RemapEngine::Settings settings;
    rkfx::ParameterWatcher watcher;

    std::vector<std::unique_ptr<InputRow>> inputRows;
    std::vector<std::unique_ptr<OutputRow>> outputRows;
    List inputList, outputList;
    juce::Viewport inputViewport, outputViewport;

    juce::ComboBox inputsShown, outputsShown;
    juce::Label inputsShownLabel, outputsShownLabel, hiddenNote;
    int lastNamesVersion = -1;

    juce::TimedCallback lightTimer { [this] { updateLights(); } };

    juce::Rectangle<int> inputsArea() const;
    juce::Rectangle<int> outputsArea() const;

    /** "In 5 · Violin": a source's name for the menus. */
    juce::String sourceText (int value) const;

    void onParameterChange();
    void refreshNamesAndViews();
    void updateLights();
    void updateHiddenNote();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKRemapEditor)
};
