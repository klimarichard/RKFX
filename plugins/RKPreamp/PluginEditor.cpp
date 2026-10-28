#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kSwitchHeight = 26;
    constexpr int kAmpHeight = kTitleHeight + kSwitchHeight + 12 + rkfx::Knob::kDefaultHeight + 8;
}

RKPreampEditor::RKPreampEditor (RKPreampProcessor& p)
    : rkfx::EditorBase (p, 640, 430)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    voicing      = std::make_unique<rkfx::SegmentedSwitch> (param ("voicing"), accent(),
                                                            juce::StringArray { "Clean", "Crunch", "Top Boost", "Hot-Rod" });
    cab          = std::make_unique<rkfx::SegmentedSwitch> (param ("cab"), accent(), juce::StringArray { "Off", "1x12", "2x12", "4x12", "Bass" });
    oversampling = std::make_unique<rkfx::SegmentedSwitch> (param ("oversampling"), accent(), juce::StringArray {}, "Oversampling");
    bright       = std::make_unique<rkfx::ToggleSwitch> (param ("bright"), accent(), "Bright");
    gain         = std::make_unique<Knob> (param ("gain"), "Gain");
    bass         = std::make_unique<Knob> (param ("bass"), "Bass");
    middle       = std::make_unique<Knob> (param ("middle"), "Middle");
    treble       = std::make_unique<Knob> (param ("treble"), "Treble");
    presence     = std::make_unique<Knob> (param ("presence"), "Presence");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             voicing.get(), cab.get(), oversampling.get(), bright.get(),
             gain.get(), bass.get(), middle.get(), treble.get(), presence.get() })
        addToCanvas (*c);

    finishSetup();
}

juce::Rectangle<int> RKPreampEditor::ampArea() const { return contentArea().removeFromTop (kAmpHeight); }
juce::Rectangle<int> RKPreampEditor::cabArea() const { return contentArea().withTrimmedTop (kAmpHeight + 10); }

void RKPreampEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = ampArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto switches = area.removeFromTop (kSwitchHeight);
        voicing->setBounds (switches.removeFromLeft (400));
        bright->setBounds (switches.removeFromRight (80));
        area.removeFromTop (12);

        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto width = knobs.getWidth() / 5;
        for (auto* k : { gain.get(), bass.get(), middle.get(), treble.get(), presence.get() })
            k->setBounds (knobs.removeFromLeft (width));
    }

    {
        auto area = cabArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        cab->setBounds (area.removeFromTop (kSwitchHeight).removeFromLeft (320));
    }

    auto global = globalArea();
    oversampling->setBounds (global.removeFromLeft (180).withSizeKeepingCentre (180, 46));
}

void RKPreampEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, ampArea(), "Amp");
    drawSection (g, cabArea(), "Cab");
}
