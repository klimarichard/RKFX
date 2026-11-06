#pragma once

#include "plugin/EditorBase.h"

class RKLoopProcessor;

class RKLoopEditor : public rkfx::EditorBase
{
public:
    explicit RKLoopEditor (RKLoopProcessor&);

private:
    RKLoopProcessor& loopProcessor;

    /** A footswitch: down while held (the processor acts on the press). */
    struct Footswitch : public juce::TextButton
    {
        Footswitch (juce::RangedAudioParameter&, const juce::String& text);
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        juce::RangedAudioParameter& param;
    };

    /** One loop: its number, role, state and progress; a click selects it. */
    struct Tile : public juce::Component
    {
        Tile (RKLoopEditor&, int index);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        RKLoopEditor& owner;
        const int index;
    };

    std::array<std::unique_ptr<Footswitch>, 6> footswitches;
    std::unique_ptr<Footswitch> saveLoops;          // Quick save all, a footswitch (assignable in RKBoard)
    juce::TextButton exportButton { "Export" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::array<std::unique_ptr<Tile>, 8> tiles;
    std::unique_ptr<rkfx::ToggleSwitch> sync, reverse, halfSpeed;
    std::unique_ptr<rkfx::SegmentedSwitch> role;    // of the selected loop
    std::unique_ptr<rkfx::Knob> level;
    int roleFor = -1;
    bool blink = false;
    int blinkCounter = 0;

    juce::Rectangle<int> loopsArea() const;
    juce::Rectangle<int> controlArea() const;
    juce::Rectangle<int> statusArea() const;
    void showRoleSwitch (int loop);
    juce::Rectangle<int> exportStatusArea() const;

    // Export (docs/design/RKLoop Export.md).
    void showExportMenu();
    void addExportLoopItem (juce::PopupMenu&, int loop, const juce::String& text);
    void chooseLoopFile (int loop);
    void chooseExportFolder();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    void refresh();
    juce::TimedCallback refresher { [this] { refresh(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKLoopEditor)
};
