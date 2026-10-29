#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kHoldHeight = 120;
    constexpr int kCaptureWidth = 300;
    constexpr int kSoundWidth = 330;
    constexpr int kTitledSwitch = 44;
}

//==============================================================================
RKHoldEditor::Footswitch::Footswitch (juce::RangedAudioParameter& p, const juce::String& text)
    : juce::TextButton (text), param (p)
{
    rkfx::tagParameter (*this, p.getParameterID());   // RKBoard: right-click to assign it
    onStateChange = [this]
    {
        const auto pressed = isDown();
        if (pressed == (param.getValue() >= 0.5f))
            return;
        param.beginChangeGesture();
        param.setValueNotifyingHost (pressed ? 1.0f : 0.0f);
        param.endChangeGesture();
    };
}

// A right-click belongs to the host (RKBoard's assign menu), not the switch.
void RKHoldEditor::Footswitch::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseDown (e);
}

void RKHoldEditor::Footswitch::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseUp (e);
}

//==============================================================================
void RKHoldEditor::LayerDisplay::update()
{
    bool changed = false;
    for (int i = 0; i < HoldEngine::kLayers; ++i)
    {
        const auto l = owner.holdProcessor.getLayer (i);
        auto& s = shown[(size_t) i];
        if (l.active != s.active || l.note != s.note || std::abs (l.level - s.level) > 0.01f)
            changed = true;
        s = l;
    }
    if (changed)
        repaint();
}

void RKHoldEditor::LayerDisplay::paint (juce::Graphics& g)
{
    using namespace rkfx;
    auto area = getLocalBounds().toFloat();
    const auto slotWidth = (area.getWidth() - 3.0f * kGap) / (float) HoldEngine::kLayers;

    for (int i = 0; i < HoldEngine::kLayers; ++i)
    {
        auto slot = area.removeFromLeft (slotWidth);
        area.removeFromLeft ((float) kGap);
        const auto& l = shown[(size_t) i];

        g.setColour (Palette::background);
        g.fillRoundedRectangle (slot, Style::kPanelRadius);
        if (l.active)
        {
            g.setColour (owner.accent().withAlpha (0.25f + 0.5f * l.level));
            g.drawRoundedRectangle (slot.reduced (0.75f), Style::kPanelRadius, 1.5f);
        }

        auto inner = slot.reduced (10.0f, 8.0f);
        g.setFont (Fonts::regular (11.0f));
        g.setColour (Palette::textDim);
        g.drawText ("Layer " + juce::String (i + 1), inner.removeFromTop (14.0f), juce::Justification::centredLeft);

        g.setFont (Fonts::bold (26.0f));
        g.setColour (l.active ? Palette::text : Palette::textDim.withAlpha (0.5f));
        const auto text = ! l.active ? juce::String ("-")
                        : l.note >= 0 ? juce::MidiMessage::getMidiNoteName (l.note, true, true, 3)
                                      : juce::String ("texture");
        if (l.active && l.note < 0)
            g.setFont (Fonts::bold (18.0f));
        g.drawText (text, inner.removeFromTop (inner.getHeight() - 12.0f), juce::Justification::centredLeft);

        const auto bar = inner.removeFromBottom (6.0f);
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (owner.accent());
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, l.level)), 3.0f);
    }
}

//==============================================================================
RKHoldEditor::RKHoldEditor (RKHoldProcessor& p)
    : rkfx::EditorBase (p, 900, 560),
      holdProcessor (p),
      holdButton (*p.findParam ("hold"), "Hold"),
      clearButton (*p.findParam ("clear"), "Clear")
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    mode      = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent(), juce::StringArray {}, "Mode");
    range     = std::make_unique<rkfx::SegmentedSwitch> (param ("range"), accent(), juce::StringArray {}, "Range");
    holdMode  = std::make_unique<rkfx::SegmentedSwitch> (param ("hold_mode"), accent(), juce::StringArray {}, "Hold Mode");
    layers    = std::make_unique<rkfx::SegmentedSwitch> (param ("layers"), accent(), juce::StringArray {}, "Layers");
    octave    = std::make_unique<rkfx::SegmentedSwitch> (param ("octave"), accent(), juce::StringArray {}, "Octave");
    movement  = std::make_unique<Knob> (param ("movement"), "Movement");
    tone      = std::make_unique<Knob> (param ("tone"), "Tone");
    width     = std::make_unique<Knob> (param ("width"), "Width");
    attack    = std::make_unique<Knob> (param ("attack"), "Attack");
    release   = std::make_unique<Knob> (param ("release"), "Release");
    holdLevel = std::make_unique<Knob> (param ("hold_level"), "Hold Level");
    dryLevel  = std::make_unique<Knob> (param ("dry_level"), "Dry Level");

    holdButton.setColour (juce::TextButton::buttonOnColourId, accent());
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &display, &holdButton, &clearButton, mode.get(), range.get(), holdMode.get(), layers.get(), octave.get(),
             movement.get(), tone.get(), width.get(), attack.get(), release.get(), holdLevel.get(), dryLevel.get() })
        addToCanvas (*c);

    finishSetup();
    meterTimer.startTimerHz (30);
}

juce::Rectangle<int> RKHoldEditor::holdArea() const     { return contentArea().withHeight (kHoldHeight); }
juce::Rectangle<int> RKHoldEditor::captureArea() const  { return contentArea().withTrimmedTop (kHoldHeight + kGap).withWidth (kCaptureWidth); }
juce::Rectangle<int> RKHoldEditor::soundArea() const    { return captureArea().withX (captureArea().getRight() + kGap).withWidth (kSoundWidth); }
juce::Rectangle<int> RKHoldEditor::envelopeArea() const
{
    const auto left = soundArea().getRight() + kGap;
    return captureArea().withX (left).withWidth (contentArea().getRight() - left);
}

void RKHoldEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = holdArea();
        auto buttons = area.removeFromRight (180);
        area.removeFromRight (kGap);
        display.setBounds (area);
        holdButton.setBounds (buttons.removeFromTop (78));
        buttons.removeFromTop (kGap);
        clearButton.setBounds (buttons);
    }

    {
        auto area = captureArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        for (auto* s : { mode.get(), range.get(), holdMode.get(), layers.get() })
        {
            s->setBounds (area.removeFromTop (kTitledSwitch));
            area.removeFromTop (6);
        }
    }

    {
        auto area = soundArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        octave->setBounds (area.removeFromTop (kTitledSwitch).withSizeKeepingCentre (180, kTitledSwitch));
        area.removeFromTop (12);
        auto row = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = row.getWidth() / 3;
        movement->setBounds (row.removeFromLeft (third));
        tone->setBounds (row.removeFromLeft (third));
        width->setBounds (row);
    }

    {
        auto area = envelopeArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto row1 = area.removeFromTop (Knob::kDefaultHeight);
        attack->setBounds (row1.removeFromLeft (row1.getWidth() / 2));
        release->setBounds (row1);
        area.removeFromTop (8);
        auto row2 = area.removeFromTop (Knob::kDefaultHeight);
        holdLevel->setBounds (row2.removeFromLeft (row2.getWidth() / 2));
        dryLevel->setBounds (row2);
    }
}

void RKHoldEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, captureArea(), "Capture");
    drawSection (g, soundArea(), "Sound");
    drawSection (g, envelopeArea(), "Envelope and Mix");
}
