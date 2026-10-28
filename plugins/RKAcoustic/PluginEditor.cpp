#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kSwitchHeight = 26;
    constexpr int kPickupHeight = kTitleHeight + kSwitchHeight + 12 + rkfx::Knob::kDefaultHeight + 8;
}

RKAcousticEditor::RKAcousticEditor (RKAcousticProcessor& p)
    : rkfx::EditorBase (p, 640, 500)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    voicing  = std::make_unique<rkfx::SegmentedSwitch> (param ("voicing"), accent(),
                                                        juce::StringArray { "Flat", "Violin", "Viola", "Guitar", "Bass" });
    phase    = std::make_unique<rkfx::ToggleSwitch> (param ("phase"), accent(), "Phase");
    body     = std::make_unique<Knob> (param ("body"), "Body");
    quack    = std::make_unique<Knob> (param ("quack"), "Quack");
    highPass = std::make_unique<Knob> (param ("highpass"), "High-Pass");
    notch    = std::make_unique<Knob> (param ("notch"), "Notch");
    compress = std::make_unique<Knob> (param ("compress"), "Compress");
    low      = std::make_unique<Knob> (param ("low"), "Low");
    mid      = std::make_unique<Knob> (param ("mid"), "Mid");
    midFreq  = std::make_unique<Knob> (param ("mid_freq"), "Mid Freq");
    treble   = std::make_unique<Knob> (param ("treble"), "Treble");
    air      = std::make_unique<Knob> (param ("air"), "Air");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             voicing.get(), phase.get(), body.get(), quack.get(), highPass.get(), notch.get(), compress.get(),
             low.get(), mid.get(), midFreq.get(), treble.get(), air.get() })
        addToCanvas (*c);

    finishSetup();
}

juce::Rectangle<int> RKAcousticEditor::pickupArea() const
{
    return contentArea().removeFromTop (kPickupHeight);
}

juce::Rectangle<int> RKAcousticEditor::eqArea() const
{
    return contentArea().withTrimmedTop (kPickupHeight + 10);
}

void RKAcousticEditor::layoutContent()
{
    using rkfx::Knob;

    const auto row = [] (juce::Rectangle<int> knobs, std::initializer_list<Knob*> list)
    {
        const auto width = knobs.getWidth() / (int) list.size();
        for (auto* k : list)
            k->setBounds (knobs.removeFromLeft (width));
    };

    {
        auto area = pickupArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto switches = area.removeFromTop (kSwitchHeight);
        voicing->setBounds (switches.removeFromLeft (400));
        phase->setBounds (switches.removeFromRight (80));
        area.removeFromTop (12);
        row (area.removeFromTop (Knob::kDefaultHeight), { body.get(), quack.get(), highPass.get(), notch.get(), compress.get() });
    }

    {
        auto area = eqArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        row (area.removeFromTop (Knob::kDefaultHeight), { low.get(), mid.get(), midFreq.get(), treble.get(), air.get() });
    }
}

void RKAcousticEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, pickupArea(), "Pickup");
    drawSection (g, eqArea(), "EQ");
}
