#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKSaturateEditor::RKSaturateEditor (RKSaturateProcessor& p)
    : rkfx::EditorBase (p, 600, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    character    = std::make_unique<rkfx::SegmentedSwitch> (param ("character"), accent());
    oversampling = std::make_unique<rkfx::SegmentedSwitch> (param ("oversampling"), accent(), juce::StringArray {}, "Oversampling");
    autoGain     = std::make_unique<rkfx::ToggleSwitch> (param ("autogain"), accent(), "Auto Gain");
    drive        = std::make_unique<Knob> (param ("drive"), "Drive");
    tone         = std::make_unique<Knob> (param ("tone"), "Tone");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             character.get(), oversampling.get(), autoGain.get(), drive.get(), tone.get() })
        addToCanvas (*c);

    finishSetup();
}

void RKSaturateEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight);
    character->setBounds (area.removeFromTop (26).withSizeKeepingCentre (330, 26));
    area.removeFromTop (12);

    auto row = area.removeFromTop (Knob::kDefaultHeight);
    drive->setBounds (row.removeFromLeft (Knob::kDefaultWidth + 16));
    tone->setBounds (row.removeFromLeft (Knob::kDefaultWidth + 16));
    row.removeFromLeft (20);
    autoGain->setBounds (row.removeFromLeft (96).withSizeKeepingCentre (96, 28));
    row.removeFromLeft (20);
    oversampling->setBounds (row.withSizeKeepingCentre (juce::jmin (180, row.getWidth()), 46));
}

void RKSaturateEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Saturation");
}
