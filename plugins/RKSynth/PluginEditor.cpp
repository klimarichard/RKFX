#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kSwitchHeight = 26;
    constexpr int kOscHeight = kTitleHeight + kSwitchHeight + 12 + rkfx::Knob::kDefaultHeight + 8;
}

RKSynthEditor::RKSynthEditor (RKSynthProcessor& p)
    : rkfx::EditorBase (p, 680, 470)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    range     = std::make_unique<rkfx::SegmentedSwitch> (param ("range"), accent(), juce::StringArray { "Bass", "Guitar", "Violin" });
    wave      = std::make_unique<rkfx::SegmentedSwitch> (param ("wave"), accent(), juce::StringArray { "Saw", "Square", "Tri" });
    octave    = std::make_unique<rkfx::SegmentedSwitch> (param ("octave"), accent(), juce::StringArray { "-2", "-1", "0", "+1" });
    sub       = std::make_unique<Knob> (param ("sub"), "Sub");
    glide     = std::make_unique<Knob> (param ("glide"), "Glide");
    dynamics  = std::make_unique<Knob> (param ("dynamics"), "Dynamics");
    threshold = std::make_unique<Knob> (param ("threshold"), "Threshold");
    cutoff    = std::make_unique<Knob> (param ("cutoff"), "Cutoff");
    resonance = std::make_unique<Knob> (param ("resonance"), "Resonance");
    envAmount = std::make_unique<Knob> (param ("env_amount"), "Env Amount");
    decay     = std::make_unique<Knob> (param ("decay"), "Decay");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             range.get(), wave.get(), octave.get(), sub.get(), glide.get(), dynamics.get(), threshold.get(),
             cutoff.get(), resonance.get(), envAmount.get(), decay.get() })
        addToCanvas (*c);

    finishSetup();
}

juce::Rectangle<int> RKSynthEditor::oscArea() const    { return contentArea().removeFromTop (kOscHeight); }
juce::Rectangle<int> RKSynthEditor::filterArea() const { return contentArea().withTrimmedTop (kOscHeight + 10); }

void RKSynthEditor::layoutContent()
{
    using rkfx::Knob;

    const auto row = [] (juce::Rectangle<int> knobs, std::initializer_list<Knob*> list)
    {
        const auto width = knobs.getWidth() / (int) list.size();
        for (auto* k : list)
            k->setBounds (knobs.removeFromLeft (width));
    };

    {
        auto area = oscArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto switches = area.removeFromTop (kSwitchHeight);
        range->setBounds (switches.removeFromLeft (210));
        switches.removeFromLeft (16);
        wave->setBounds (switches.removeFromLeft (210));
        octave->setBounds (switches.removeFromRight (180));
        area.removeFromTop (12);
        row (area.removeFromTop (Knob::kDefaultHeight), { sub.get(), glide.get(), dynamics.get(), threshold.get() });
    }

    {
        auto area = filterArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        row (area.removeFromTop (Knob::kDefaultHeight), { cutoff.get(), resonance.get(), envAmount.get(), decay.get() });
    }
}

void RKSynthEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, oscArea(), "Oscillator");
    drawSection (g, filterArea(), "Filter");
}
