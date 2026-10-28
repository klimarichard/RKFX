#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKSwellEditor::RKSwellEditor (RKSwellProcessor& p)
    : rkfx::EditorBase (p, 560, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    curve     = std::make_unique<rkfx::SegmentedSwitch> (param ("curve"), accent(), juce::StringArray { "Linear", "Exp", "S-Curve" });
    link      = std::make_unique<rkfx::ToggleSwitch> (param ("link"), accent(), "Link");
    threshold = std::make_unique<Knob> (param ("threshold"), "Threshold");
    time      = std::make_unique<Knob> (param ("time"), "Swell Time");
    retrigger = std::make_unique<Knob> (param ("retrigger"), "Retrigger");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             curve.get(), link.get(), threshold.get(), time.get(), retrigger.get() })
        addToCanvas (*c);

    finishSetup();
}

void RKSwellEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight);
    auto switches = area.removeFromTop (26);
    curve->setBounds (switches.removeFromLeft (240));
    link->setBounds (switches.removeFromRight (64));
    area.removeFromTop (12);

    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto third = knobs.getWidth() / 3;
    threshold->setBounds (knobs.removeFromLeft (third));
    time->setBounds (knobs.removeFromLeft (third));
    retrigger->setBounds (knobs);
}

void RKSwellEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Swell");
}
