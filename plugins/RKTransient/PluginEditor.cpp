#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kShapeWidth = 340;
}

RKTransientEditor::RKTransientEditor (RKTransientProcessor& p)
    : rkfx::EditorBase (p, 640, 420)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    attack      = std::make_unique<Knob> (param ("attack"), "Attack");
    sustain     = std::make_unique<Knob> (param ("sustain"), "Sustain");
    attackTime  = std::make_unique<Knob> (param ("attack_time"), "Attack Time");
    sustainTime = std::make_unique<Knob> (param ("sustain_time"), "Sustain Time");
    focusHpf    = std::make_unique<Knob> (param ("focus_hpf"), "High-Pass");
    focusLpf    = std::make_unique<Knob> (param ("focus_lpf"), "Low-Pass");
    listen      = std::make_unique<rkfx::ToggleSwitch> (param ("focus_listen"), rkfx::Palette::warning, "Listen");
    link        = std::make_unique<rkfx::ToggleSwitch> (param ("link"), accent(), "Link");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             attack.get(), sustain.get(), attackTime.get(), sustainTime.get(), focusHpf.get(), focusLpf.get(),
             listen.get(), link.get() })
        addToCanvas (*c);

    finishSetup();
}

juce::Rectangle<int> RKTransientEditor::shapeArea() const { return contentArea().withWidth (kShapeWidth); }
juce::Rectangle<int> RKTransientEditor::focusArea() const { return contentArea().withTrimmedLeft (kShapeWidth + kGap); }

void RKTransientEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = shapeArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 34);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto quarter = knobs.getWidth() / 4;
        attack->setBounds (knobs.removeFromLeft (quarter));
        sustain->setBounds (knobs.removeFromLeft (quarter));
        attackTime->setBounds (knobs.removeFromLeft (quarter));
        sustainTime->setBounds (knobs);
    }

    {
        auto area = focusArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto toggles = area.removeFromTop (26);
        listen->setBounds (toggles.removeFromLeft (toggles.getWidth() / 2 - 3));
        toggles.removeFromLeft (6);
        link->setBounds (toggles);
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        focusHpf->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        focusLpf->setBounds (knobs);
    }
}

void RKTransientEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, shapeArea(), "Shape");
    drawSection (g, focusArea(), "Focus");
}
