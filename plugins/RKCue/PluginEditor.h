#pragma once

#include "plugin/EditorBase.h"

class RKCueProcessor;

class RKCueEditor : public rkfx::EditorBase
{
public:
    explicit RKCueEditor (RKCueProcessor&);

private:
    RKCueProcessor& cue;
    int selected = 0;

    /** A button held like a footswitch: the processor acts on the press (and Play on the release). */
    struct Footswitch : public juce::TextButton
    {
        Footswitch (juce::RangedAudioParameter&, const juce::String& text);
        juce::RangedAudioParameter& param;
    };

    /** The whole MIDI keyboard with every slot's zone drawn above it:
        drag a zone's ends for Low and High, its dot for the Root. */
    struct KeyMap : public juce::Component
    {
        explicit KeyMap (RKCueEditor& e) : editor (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        RKCueEditor& editor;
        enum class Grab { none, low, high, root };
        Grab grab = Grab::none;
        int grabSlot = -1;
        juce::RangedAudioParameter* grabParam = nullptr;

        static constexpr int kLaneHeight = 9;
        static constexpr int kLanes = 4;
        juce::Rectangle<float> keysArea() const;
        float keyX (int key) const;              // centre of a key
        int noteAt (float x) const;
        juce::Rectangle<float> zoneBar (int slot) const;
        std::array<int, 16> lanes() const;
    };

    /** 16 pads: click to select, ▶ to play, drop a MIDI file to load. */
    struct Pads : public juce::Component, public juce::FileDragAndDropTarget
    {
        explicit Pads (RKCueEditor& e) : editor (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool isInterestedInFileDrag (const juce::StringArray&) override;
        void fileDragEnter (const juce::StringArray&, int x, int y) override { dropSlot = padAt ({ x, y }); repaint(); }
        void fileDragMove (const juce::StringArray&, int x, int y) override  { dropSlot = padAt ({ x, y }); repaint(); }
        void fileDragExit (const juce::StringArray&) override                { dropSlot = -1; repaint(); }
        void filesDropped (const juce::StringArray&, int x, int y) override;

        RKCueEditor& editor;
        int dropSlot = -1, playing = -1;
        juce::Rectangle<int> padBounds (int slot) const;
        juce::Rectangle<int> playBounds (int slot) const;
        int padAt (juce::Point<int>) const;
    };

    KeyMap keyMap { *this };
    Pads pads { *this };

    // The selected slot.
    juce::Label nameLabel;
    juce::TextButton loadButton { "Load" }, clearButton { "Clear" }, learnButton { "Learn" };
    std::unique_ptr<rkfx::Knob> low, high, root, transpose, velocity, channel;
    std::unique_ptr<rkfx::SegmentedSwitch> mode;
    std::unique_ptr<rkfx::ToggleSwitch> trackPitch, keepDrums, loop, poly;
    std::unique_ptr<Footswitch> stopSlot;
    std::unique_ptr<juce::ParameterAttachment> pitchWatcher;

    // The bottom row.
    std::unique_ptr<rkfx::SegmentedSwitch> tempoSource;
    std::unique_ptr<rkfx::Knob> tempo, trigChannel;
    std::unique_ptr<rkfx::ToggleSwitch> thru;
    std::unique_ptr<Footswitch> stopAll;
    std::unique_ptr<juce::ParameterAttachment> tempoWatcher;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String note;

    // Learn: the next key sets the zone (Shift-click: the next two keys set a range).
    enum class Learn { off, one, rangeFirst, rangeSecond };
    Learn learn = Learn::off;
    int learnNoteCount = 0, learnFirst = 0;

    int shownVersion = -1;
    std::array<int, 16> shownCopies {};
    std::array<float, 16> shownProgress {};
    std::array<uint64_t, 2> shownKeys {};
    std::vector<float> shownZones;

    juce::RangedAudioParameter& slotParam (int slot, const char* name) const;
    void select (int slot);
    void buildSlotControls();
    void showSlotName();
    void loadInto (int slot, const juce::File&);
    void setNote (int slot, const char* name, int noteNumber);
    void updateLearnButton();

    juce::Rectangle<int> mapArea() const;
    juce::Rectangle<int> padsArea() const;
    juce::Rectangle<int> slotArea() const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    void refresh();
    juce::TimedCallback refresher { [this] { refresh(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKCueEditor)
};
