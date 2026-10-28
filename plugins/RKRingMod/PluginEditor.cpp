#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kSwitchHeight = 26;
    constexpr int kCarrierWidth = 400;
    constexpr int kGap = 10;
}

RKRingModEditor::RKRingModEditor (RKRingModProcessor& p)
    : rkfx::EditorBase (p, 640, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    mode      = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent(), juce::StringArray { "Free", "Tracked", "Synced" });
    wave      = std::make_unique<rkfx::SegmentedSwitch> (param ("wave"), accent(), juce::StringArray { "Sine", "Tri", "Square" });
    range     = std::make_unique<rkfx::SegmentedSwitch> (param ("range"), accent(), juce::StringArray { "Bass", "Guitar", "Violin" });
    frequency = std::make_unique<Knob> (param ("frequency"), "Frequency");
    interval  = std::make_unique<Knob> (param ("interval"), "Interval");
    division  = std::make_unique<Knob> (param ("division"), "Division");
    lfoDepth  = std::make_unique<Knob> (param ("lfo_depth"), "Depth");
    lfoRate   = std::make_unique<Knob> (param ("lfo_rate"), "Rate");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             mode.get(), wave.get(), range.get(), frequency.get(), interval.get(), division.get(), lfoDepth.get(), lfoRate.get() })
        addToCanvas (*c);

    // The carrier's control follows the mode: Frequency, Interval (and the
    // range to track) or Division.
    modeWatcher = std::make_unique<juce::ParameterAttachment> (param ("mode"), [this] (float v)
    {
        const auto m = juce::roundToInt (v);
        frequency->setVisible (m == 0);
        interval->setVisible (m == 1);
        range->setVisible (m == 1);
        division->setVisible (m == 2);
    });
    depthWatcher = std::make_unique<juce::ParameterAttachment> (param ("lfo_depth"), [this] (float v)
    {
        lfoRate->setDimmed (v < 0.5f);
    });
    modeWatcher->sendInitialUpdate();
    depthWatcher->sendInitialUpdate();

    finishSetup();
}

juce::Rectangle<int> RKRingModEditor::carrierArea() const { return contentArea().withWidth (kCarrierWidth); }
juce::Rectangle<int> RKRingModEditor::lfoArea() const     { return contentArea().withTrimmedLeft (kCarrierWidth + kGap); }

void RKRingModEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = carrierArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto switches = area.removeFromTop (kSwitchHeight);
        mode->setBounds (switches.removeFromLeft (200));
        wave->setBounds (switches.removeFromRight (160));
        area.removeFromTop (8);
        range->setBounds (area.removeFromTop (kSwitchHeight).removeFromLeft (200));
        area.removeFromTop (8);
        const auto knob = area.removeFromTop (Knob::kDefaultHeight).withSizeKeepingCentre (Knob::kDefaultWidth + 40, Knob::kDefaultHeight);
        for (auto* k : { frequency.get(), interval.get(), division.get() })
            k->setBounds (knob);
    }

    {
        auto area = lfoArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        area.removeFromTop (kSwitchHeight + 8 + kSwitchHeight + 8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        lfoDepth->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        lfoRate->setBounds (knobs);
    }
}

void RKRingModEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, carrierArea(), "Carrier");
    drawSection (g, lfoArea(), "LFO");
}
