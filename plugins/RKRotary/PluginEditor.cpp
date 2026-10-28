#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
}

RKRotaryEditor::RKRotaryEditor (RKRotaryProcessor& p)
    : rkfx::EditorBase (p, 740, 450)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    speed        = std::make_unique<rkfx::SegmentedSwitch> (param ("speed"), accent());
    oversampling = std::make_unique<rkfx::SegmentedSwitch> (param ("oversampling"), accent(), juce::StringArray {}, "Drive Oversampling");
    slowRate     = std::make_unique<Knob> (param ("slow_rate"), "Slow Rate");
    fastRate     = std::make_unique<Knob> (param ("fast_rate"), "Fast Rate");
    hornAccel    = std::make_unique<Knob> (param ("horn_accel"), "Horn Accel");
    drumAccel    = std::make_unique<Knob> (param ("drum_accel"), "Drum Accel");
    balance      = std::make_unique<Knob> (param ("balance"), "Drum/Horn");
    distance     = std::make_unique<Knob> (param ("distance"), "Mic Distance");
    width        = std::make_unique<Knob> (param ("width"), "Width");
    drive        = std::make_unique<Knob> (param ("drive"), "Drive");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             speed.get(), oversampling.get(), slowRate.get(), fastRate.get(), hornAccel.get(), drumAccel.get(),
             balance.get(), distance.get(), width.get(), drive.get() })
        addToCanvas (*c);

    finishSetup();
}

juce::Rectangle<int> RKRotaryEditor::rotorArea() const { return contentArea().withWidth ((contentArea().getWidth() - kGap) / 2); }
juce::Rectangle<int> RKRotaryEditor::soundArea() const { return contentArea().withTrimmedLeft ((contentArea().getWidth() - kGap) / 2 + kGap); }

void RKRotaryEditor::layoutContent()
{
    using rkfx::Knob;

    const auto rowOfFour = [] (juce::Rectangle<int> row, std::initializer_list<Knob*> knobs)
    {
        const auto w = row.getWidth() / 4;
        for (auto* k : knobs)
            k->setBounds (row.removeFromLeft (w));
    };

    {
        auto area = rotorArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        speed->setBounds (area.removeFromTop (34));
        area.removeFromTop (10);
        rowOfFour (area.removeFromTop (Knob::kDefaultHeight), { slowRate.get(), fastRate.get(), hornAccel.get(), drumAccel.get() });
    }

    {
        auto area = soundArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 44);
        rowOfFour (area.removeFromTop (Knob::kDefaultHeight), { balance.get(), distance.get(), width.get(), drive.get() });
    }

    oversampling->setBounds (globalArea().removeFromLeft (190).withSizeKeepingCentre (190, 46));
}

void RKRotaryEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, rotorArea(), "Rotors");
    drawSection (g, soundArea(), "Sound");
}
