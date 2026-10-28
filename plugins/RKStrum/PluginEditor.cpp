#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKStrumEditor::RKStrumEditor (RKStrumProcessor& p)
    : rkfx::EditorBase (p, 600, 330)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    direction = std::make_unique<rkfx::SegmentedSwitch> (param ("direction"), accent(), juce::StringArray { "Down", "Up", "Alternate" });
    speed     = std::make_unique<Knob> (param ("speed"), "Speed");
    window    = std::make_unique<Knob> (param ("window"), "Chord Window");
    ramp      = std::make_unique<Knob> (param ("ramp"), "Velocity Ramp");
    humanize  = std::make_unique<Knob> (param ("humanize"), "Humanize");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             direction.get(), speed.get(), window.get(), ramp.get(), humanize.get() })
        addToCanvas (*c);

    finishSetup();
}

void RKStrumEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight);
    direction->setBounds (area.removeFromTop (26).removeFromLeft (300));
    area.removeFromTop (12);
    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto quarter = knobs.getWidth() / 4;
    speed->setBounds (knobs.removeFromLeft (quarter));
    ramp->setBounds (knobs.removeFromLeft (quarter));
    humanize->setBounds (knobs.removeFromLeft (quarter));
    window->setBounds (knobs);
}

void RKStrumEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Strum");
}
