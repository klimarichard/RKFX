#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kKeyWidth = 220;
    constexpr int kGap = 10;
}

RKHarmonizeEditor::RKHarmonizeEditor (RKHarmonizeProcessor& p)
    : rkfx::EditorBase (p, 640, 330)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    key      = std::make_unique<Knob> (param ("key"), "Key");
    scale    = std::make_unique<Knob> (param ("scale"), "Scale");
    voice1   = std::make_unique<Knob> (param ("voice1"), "Voice 1");
    voice2   = std::make_unique<Knob> (param ("voice2"), "Voice 2");
    velocity = std::make_unique<Knob> (param ("velocity"), "Velocity");
    original = std::make_unique<rkfx::ToggleSwitch> (param ("original"), accent(), "Original");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             key.get(), scale.get(), voice1.get(), voice2.get(), velocity.get(), original.get() })
        addToCanvas (*c);

    finishSetup();
}

juce::Rectangle<int> RKHarmonizeEditor::keyArea() const     { return contentArea().withWidth (kKeyWidth); }
juce::Rectangle<int> RKHarmonizeEditor::harmonyArea() const { return contentArea().withTrimmedLeft (kKeyWidth + kGap); }

void RKHarmonizeEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = keyArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        key->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        scale->setBounds (knobs);
    }

    {
        auto area = harmonyArea().reduced (8, 0);
        auto title = area.removeFromTop (kTitleHeight + 8);
        original->setBounds (title.removeFromRight (84).withSizeKeepingCentre (84, 24));
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        voice1->setBounds (knobs.removeFromLeft (third));
        voice2->setBounds (knobs.removeFromLeft (third));
        velocity->setBounds (knobs);
    }
}

void RKHarmonizeEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, keyArea(), "Key");
    drawSection (g, harmonyArea(), "Harmony");
}
