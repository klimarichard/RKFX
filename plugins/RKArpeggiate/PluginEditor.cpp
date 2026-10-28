#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kSwitchHeight = 26;
    constexpr int kPatternHeight = kTitleHeight + kSwitchHeight + 12 + rkfx::Knob::kDefaultHeight + 8;
}

RKArpeggiateEditor::RKArpeggiateEditor (RKArpeggiateProcessor& p)
    : rkfx::EditorBase (p, 680, 460)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    pattern      = std::make_unique<rkfx::SegmentedSwitch> (param ("pattern"), accent(),
                                                            juce::StringArray { "Up", "Down", "Up/Down", "Played", "Random", "Chord" });
    latch        = std::make_unique<rkfx::ToggleSwitch> (param ("latch"), accent(), "Latch");
    division     = std::make_unique<Knob> (param ("division"), "Rate");
    octaves      = std::make_unique<Knob> (param ("octaves"), "Octaves");
    gate         = std::make_unique<Knob> (param ("gate"), "Gate");
    swing        = std::make_unique<Knob> (param ("swing"), "Swing");
    velocityMode = std::make_unique<rkfx::SegmentedSwitch> (param ("velocity_mode"), accent(), juce::StringArray { "As Played", "Fixed" });
    velocity     = std::make_unique<Knob> (param ("velocity"), "Velocity");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             pattern.get(), latch.get(), division.get(), octaves.get(), gate.get(), swing.get(), velocityMode.get(), velocity.get() })
        addToCanvas (*c);

    velocityWatcher = std::make_unique<juce::ParameterAttachment> (param ("velocity_mode"), [this] (float v)
    {
        velocity->setDimmed (v < 0.5f);
    });
    velocityWatcher->sendInitialUpdate();

    finishSetup();
}

juce::Rectangle<int> RKArpeggiateEditor::patternArea() const { return contentArea().removeFromTop (kPatternHeight); }
juce::Rectangle<int> RKArpeggiateEditor::feelArea() const    { return contentArea().withTrimmedTop (kPatternHeight + 10); }

void RKArpeggiateEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = patternArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto switches = area.removeFromTop (kSwitchHeight);
        pattern->setBounds (switches.removeFromLeft (440));
        latch->setBounds (switches.removeFromRight (80));
        area.removeFromTop (12);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto quarter = knobs.getWidth() / 4;
        division->setBounds (knobs.removeFromLeft (quarter));
        octaves->setBounds (knobs.removeFromLeft (quarter));
        gate->setBounds (knobs.removeFromLeft (quarter));
        swing->setBounds (knobs);
    }

    {
        auto area = feelArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto row = area.removeFromTop (Knob::kDefaultHeight);
        velocityMode->setBounds (row.removeFromLeft (200).withSizeKeepingCentre (200, kSwitchHeight));
        row.removeFromLeft (16);
        velocity->setBounds (row.removeFromLeft (150));
    }
}

void RKArpeggiateEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, patternArea(), "Pattern");
    drawSection (g, feelArea(), "Velocity");
}
