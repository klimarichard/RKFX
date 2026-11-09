#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rkfx { class ProcessorBase; }

/** An insert's own window, laid out for the iPad (the console remote's insert
    view): where each of its controls sits, and the window drawn without them
    (its panels, titles, curves and meters) as a picture to put under the
    iPad's own knobs, faders and switches. So the iPad shows the plugin with
    the layout of the computer's window.

    It keeps a hidden editor of the plugin (not on screen, not the plugin's
    active editor) while an iPad has the insert open, and notices when its
    layout changes (RKGraphEQ's band count shows other faders) and when the
    picture does (a curve, a meter).

    Message thread only. The editor must go before the plugin: RKMix's
    console drops the layout when an insert is about to be removed. */
class MixInsertLayout
{
public:
    /** @p slot is the insert's flat slot number (for the iPad's parameter IDs, "ins:<slot>:<id>"). */
    MixInsertLayout (rkfx::ProcessorBase& plugin, int slot);
    ~MixInsertLayout();

    rkfx::ProcessorBase& plugin;
    const int slot;

    /** False for a plugin whose editor isn't one of the suite's (no layout: the iPad lists its controls). */
    bool hasLayout() const noexcept { return editor != nullptr; }

    /** The layout as a JSON object: {"w", "h", "img" (data URL), "ctl": [...], "more": [IDs not in the window]}. */
    juce::String layoutJson();

    /** What changed since the last call (or since layoutJson()): "layout" (send it all again),
        "image" (only the picture: imageUrl()), or empty. Draws the window, so call it a few times a second at most. */
    juce::String update();

    /** The latest picture as a data URL. */
    const juce::String& imageUrl() const noexcept { return image; }

    /** The picture's scale (2: sharp on the iPad's screen). */
    static constexpr float kScale = 2.0f;

private:
    struct Control
    {
        juce::Component* component = nullptr;
        juce::String id, widget, label, title;
        juce::StringArray labels;
        juce::Rectangle<int> bounds;   // in the window, below its header
        bool dimmed = false, compact = false;
        float zero = 0.0f;             // a bipolar fader's 0 (normalised)
        float pickValue = 0.0f;        // a pick area's value (normalised)
    };

    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::Rectangle<int> area;         // the window without its header (presets: the computer's)
    std::vector<Control> controls;     // shown now
    juce::StringArray tagged;          // every control's parameter, shown or not
    juce::String signature, image;
    juce::uint64 imageHash = 0;        // FNV-1a of the pixels

    void scan();
    juce::String signatureOf() const;
    bool drawImage();                  // true if the picture changed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixInsertLayout)
};
