#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kDriveWidth = 250;
}

RKDistortEditor::RKDistortEditor (RKDistortProcessor& p)
    : rkfx::EditorBase (p, 620, 420)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    type         = std::make_unique<rkfx::SegmentedSwitch> (param ("type"), accent(), juce::StringArray { "Drive", "Dist", "Fuzz" });
    cab          = std::make_unique<rkfx::SegmentedSwitch> (param ("cab"), accent(), juce::StringArray { "Off", "1x12", "2x12", "4x12", "Bass" });
    oversampling = std::make_unique<rkfx::SegmentedSwitch> (param ("oversampling"), accent(), juce::StringArray {}, "Oversampling");
    autoGain     = std::make_unique<rkfx::ToggleSwitch> (param ("autogain"), accent(), "Auto Gain");
    gain         = std::make_unique<Knob> (param ("gain"), "Gain");
    tight        = std::make_unique<Knob> (param ("tight"), "Tight");
    mid          = std::make_unique<Knob> (param ("mid"), "Mid");
    tone         = std::make_unique<Knob> (param ("tone"), "Tone");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             type.get(), cab.get(), oversampling.get(), autoGain.get(), gain.get(), tight.get(), mid.get(), tone.get() })
        addToCanvas (*c);

    finishSetup();
}

juce::Rectangle<int> RKDistortEditor::driveArea() const { return contentArea().withWidth (kDriveWidth); }
juce::Rectangle<int> RKDistortEditor::toneArea() const  { return contentArea().withTrimmedLeft (kDriveWidth + kGap); }

void RKDistortEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = driveArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        type->setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        gain->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        tight->setBounds (knobs);
    }

    {
        auto area = toneArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        cab->setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight).withSizeKeepingCentre (2 * Knob::kDefaultWidth + 40, Knob::kDefaultHeight);
        mid->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        tone->setBounds (knobs);
    }

    auto global = globalArea();
    autoGain->setBounds (global.removeFromLeft (96).withSizeKeepingCentre (96, 28));
    global.removeFromLeft (16);
    oversampling->setBounds (global.removeFromLeft (180).withSizeKeepingCentre (180, 46));
}

void RKDistortEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, driveArea(), "Drive");
    drawSection (g, toneArea(), "Tone & Cab");
}
