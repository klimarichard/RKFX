#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ProcessorBase.h"
#include "gui/Knob.h"
#include "gui/LookAndFeel.h"
#include "gui/Switches.h"

namespace rkfx
{

/** The base every RKFX editor derives from. Lays out what every plugin
    shares, in the same place every time:

        +--------------------------------------------------------------+
        | RKName                         [Preset name * v] [Save] [As] |  header
        +--------------------------------------------------------------+
        |                                                              |
        |                 content (the plugin's sections)              |
        |                                                              |
        +--------------------------------------------------------------+
        | global controls (plugin)          [Dry/Wet] [Output] [Byp]   |  bottom row
        +--------------------------------------------------------------+

    The plugin designs at a fixed base size and places its components (added
    with addToCanvas) inside contentArea() and globalArea() in layoutContent().
    The whole canvas is scaled as one, so the window resizes with a fixed
    aspect ratio and stays sharp on any display.

    A derived editor must call finishSetup() at the end of its constructor. */
class EditorBase : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    EditorBase (ProcessorBase&, int baseWidth, int baseHeight);
    ~EditorBase() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kHeaderHeight = 44;
    static constexpr int kBottomHeight = 112;
    static constexpr int kMargin = 12;

protected:
    void addToCanvas (juce::Component& c) { canvas.addAndMakeVisible (c); }

    juce::Rectangle<int> contentArea() const;
    juce::Rectangle<int> globalArea() const;

    juce::Colour accent() const noexcept { return proc.getConfig().accent; }

    /** Place the plugin's components (base-size coordinates). */
    virtual void layoutContent() {}

    /** Extra drawing on the canvas, under the components (section titles, frames). */
    virtual void paintContent (juce::Graphics&) {}

    /** Lays everything out; call once at the end of the derived constructor. */
    void finishSetup();

    /** A titled frame around a group of controls, drawn in paintContent(). */
    static void drawSection (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title);

    ProcessorBase& proc;

private:
    struct Canvas : public juce::Component
    {
        explicit Canvas (EditorBase& o) : owner (o) { setOpaque (true); }
        void paint (juce::Graphics& g) override { owner.paintCanvas (g); }
        EditorBase& owner;
    };

    LookAndFeel lookAndFeel;
    const int baseWidth, baseHeight;
    Canvas canvas { *this };

    juce::TextButton presetButton, saveButton { "Save" }, saveAsButton { "Save as" };
    juce::Label noteLabel;
    std::unique_ptr<Knob> mixKnob, outputKnob;
    std::unique_ptr<ToggleSwitch> bypassSwitch;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastPresetCounter = -1;

    void paintCanvas (juce::Graphics&);
    void layoutBase();

    void refreshPresetButton();
    void showNote (const juce::String& text, bool isError);
    void showPresetMenu();
    void save();
    void saveAs();
    void chooseLibraryFolder();

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorBase)
};

} // namespace rkfx
