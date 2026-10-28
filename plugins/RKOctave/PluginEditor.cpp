#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKOctaveEditor::RKOctaveEditor (RKOctaveProcessor& p)
    : rkfx::EditorBase (p, 600, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    range     = std::make_unique<rkfx::SegmentedSwitch> (param ("range"), accent(), juce::StringArray { "Bass", "Guitar", "Violin" });
    voice     = std::make_unique<rkfx::SegmentedSwitch> (param ("voice"), accent(), juce::StringArray { "Natural", "Synth" });
    direct    = std::make_unique<Knob> (param ("direct"), "Direct");
    octave1   = std::make_unique<Knob> (param ("octave1"), "Octave 1");
    octave2   = std::make_unique<Knob> (param ("octave2"), "Octave 2");
    tone      = std::make_unique<Knob> (param ("tone"), "Tone");
    threshold = std::make_unique<Knob> (param ("threshold"), "Threshold");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             range.get(), voice.get(), direct.get(), octave1.get(), octave2.get(), tone.get(), threshold.get() })
        addToCanvas (*c);

    finishSetup();
}

void RKOctaveEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight);
    auto switches = area.removeFromTop (26);
    range->setBounds (switches.removeFromLeft (240));
    voice->setBounds (switches.removeFromRight (170));
    area.removeFromTop (12);

    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto fifth = knobs.getWidth() / 5;
    for (auto* k : { direct.get(), octave1.get(), octave2.get(), tone.get() })
        k->setBounds (knobs.removeFromLeft (fifth));
    threshold->setBounds (knobs);
}

void RKOctaveEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Octave");
}
