#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kDisplayHeight = 110;
    constexpr int kRowHeight = 130;
    constexpr int kModeWidth = 260;
    constexpr int kPedalWidth = 370;
    constexpr int kTitledSwitch = 44;

    juce::String semitoneText (float st)
    {
        const auto rounded = std::round (st * 10.0f) / 10.0f;
        return (rounded > 0.0f ? "+" : "") + juce::String (rounded, std::abs (rounded - std::round (rounded)) < 0.05f ? 0 : 1) + " st";
    }
}

//==============================================================================
RKPolyPitchEditor::Footswitch::Footswitch (juce::RangedAudioParameter& p, const juce::String& text)
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
void RKPolyPitchEditor::Footswitch::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseDown (e);
}

void RKPolyPitchEditor::Footswitch::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseUp (e);
}

//==============================================================================
void RKPolyPitchEditor::PedalDisplay::update()
{
    const auto s = owner.poly.shownShift.load(), p = owner.poly.shownPosition.load();
    if (std::abs (s - shift) > 0.02f || std::abs (p - position) > 0.002f)
    {
        shift = s;
        position = p;
        repaint();
    }
}

void RKPolyPitchEditor::PedalDisplay::paint (juce::Graphics& g)
{
    using namespace rkfx;
    const auto& s = owner.settings;
    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::background);
    g.fillRoundedRectangle (area, Style::kPanelRadius);
    area = area.reduced (16.0f, 10.0f);

    // The shift now, large.
    auto left = area.removeFromLeft (200.0f);
    g.setColour (Palette::text);
    g.setFont (Fonts::bold (44.0f));
    g.drawText (semitoneText (s.mode == PolyPitchEngine::Mode::detune ? 0.0f : shift), left.removeFromTop (58.0f), juce::Justification::centredLeft);
    g.setFont (Fonts::regular (12.0f));
    g.setColour (s.mode == PolyPitchEngine::Mode::whammy ? Palette::warning : Palette::textDim);
    const auto delay = PolyPitchEngine::delayMs (s.range, s.lowLatency);
    g.drawText (s.mode == PolyPitchEngine::Mode::whammy
                    ? "Your sound: delayed about " + juce::String (juce::roundToInt (delay)) + " ms"
                    : juce::String ("Your sound: no delay"),
                left.removeFromTop (18.0f), juce::Justification::centredLeft);

    area.removeFromLeft (24.0f);

    // The treadle: heel on the left, toe on the right, the position now.
    auto treadle = area.withSizeKeepingCentre (area.getWidth(), 20.0f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (treadle, 10.0f);
    g.setColour (owner.accent().withAlpha (0.5f));
    g.fillRoundedRectangle (treadle.withWidth (juce::jmax (20.0f, treadle.getWidth() * position)), 10.0f);
    const auto x = treadle.getX() + treadle.getWidth() * juce::jlimit (0.0f, 1.0f, position);
    g.setColour (owner.accent());
    g.fillEllipse (x - 11.0f, treadle.getCentreY() - 11.0f, 22.0f, 22.0f);

    g.setFont (Fonts::regular (12.0f));
    g.setColour (Palette::textDim);
    const auto labels = treadle.translated (0.0f, 22.0f).withHeight (16.0f);
    g.drawText ("Heel " + semitoneText (s.heel), labels, juce::Justification::centredLeft);
    g.drawText ("Toe " + semitoneText (s.toe), labels, juce::Justification::centredRight);
}

//==============================================================================
RKPolyPitchEditor::RKPolyPitchEditor (RKPolyPitchProcessor& p)
    : rkfx::EditorBase (p, 980, 560),
      poly (p),
      settings (p.currentSettings()),
      momentary (*p.findParam ("momentary"), "Momentary"),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    mode       = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent(), juce::StringArray {}, "Mode");
    range      = std::make_unique<rkfx::SegmentedSwitch> (param ("range"), accent(), juce::StringArray {}, "Range");
    lowLatency = std::make_unique<rkfx::ToggleSwitch> (param ("low_latency"), accent(), "Low Latency");
    heel       = std::make_unique<Knob> (param ("heel"), "Heel");
    toe        = std::make_unique<Knob> (param ("toe"), "Toe");
    pedal      = std::make_unique<Knob> (param ("pedal"), "Pedal");
    glide      = std::make_unique<Knob> (param ("glide"), "Glide");
    rise       = std::make_unique<Knob> (param ("rise"), "Rise");
    fall       = std::make_unique<Knob> (param ("fall"), "Fall");
    fine       = std::make_unique<Knob> (param ("fine"), "Fine");
    detune     = std::make_unique<Knob> (param ("detune"), "Detune");
    voiceLevel = std::make_unique<Knob> (param ("voice_level"), "Voice Level");
    width      = std::make_unique<Knob> (param ("width"), "Width");
    tone       = std::make_unique<Knob> (param ("tone"), "Tone");

    momentary.setColour (juce::TextButton::buttonOnColourId, accent());
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &display, &momentary, mode.get(), range.get(), lowLatency.get(), heel.get(), toe.get(), pedal.get(), glide.get(),
             rise.get(), fall.get(), fine.get(), detune.get(), voiceLevel.get(), width.get(), tone.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
    meterTimer.startTimerHz (30);
}

void RKPolyPitchEditor::onParameterChange()
{
    settings = poly.currentSettings();
    const auto detuning = settings.mode == PolyPitchEngine::Mode::detune;
    for (auto* k : { heel.get(), toe.get(), pedal.get(), glide.get(), rise.get(), fall.get(), fine.get() })
        k->setDimmed (detuning);
    detune->setDimmed (! detuning);
    width->setDimmed (! detuning);
    voiceLevel->setDimmed (settings.mode == PolyPitchEngine::Mode::whammy);
    display.repaint();
}

juce::Rectangle<int> RKPolyPitchEditor::displayArea() const   { return contentArea().withHeight (kDisplayHeight); }
juce::Rectangle<int> RKPolyPitchEditor::modeArea() const      { return contentArea().withTrimmedTop (kDisplayHeight + kGap).withHeight (kRowHeight).withWidth (kModeWidth); }
juce::Rectangle<int> RKPolyPitchEditor::pedalArea() const     { return modeArea().withX (modeArea().getRight() + kGap).withWidth (kPedalWidth); }
juce::Rectangle<int> RKPolyPitchEditor::momentaryArea() const
{
    const auto left = pedalArea().getRight() + kGap;
    return modeArea().withX (left).withWidth (contentArea().getRight() - left);
}
juce::Rectangle<int> RKPolyPitchEditor::voiceArea() const     { return contentArea().withTrimmedTop (kDisplayHeight + kRowHeight + 2 * kGap); }

void RKPolyPitchEditor::layoutContent()
{
    using rkfx::Knob;
    display.setBounds (displayArea());

    auto knobRow = [] (juce::Rectangle<int> row, std::initializer_list<Knob*> knobs)
    {
        const auto w = row.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
            k->setBounds (row.removeFromLeft (w));
    };

    {
        auto area = modeArea().reduced (8, 0);
        auto title = area.removeFromTop (kTitleHeight + 2);
        lowLatency->setBounds (title.removeFromRight (100).withSizeKeepingCentre (100, 22));
        mode->setBounds (area.removeFromTop (kTitledSwitch));
        area.removeFromTop (4);
        range->setBounds (area.removeFromTop (kTitledSwitch));
    }
    {
        auto area = pedalArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        knobRow (area.removeFromTop (Knob::kDefaultHeight), { heel.get(), toe.get(), pedal.get(), glide.get() });
    }
    {
        auto area = momentaryArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto row = area.removeFromTop (Knob::kDefaultHeight);
        momentary.setBounds (row.removeFromLeft (110).withSizeKeepingCentre (110, 60));
        row.removeFromLeft (8);
        knobRow (row, { rise.get(), fall.get() });
    }
    {
        auto area = voiceArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        knobRow (area.removeFromTop (Knob::kDefaultHeight), { fine.get(), detune.get(), voiceLevel.get(), width.get(), tone.get() });
    }
}

void RKPolyPitchEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, modeArea(), "Mode");
    drawSection (g, pedalArea(), "Pedal");
    drawSection (g, momentaryArea(), "Momentary");
    drawSection (g, voiceArea(), "Voice");
}
