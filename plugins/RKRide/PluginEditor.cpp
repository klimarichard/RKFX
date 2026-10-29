#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kRiderHeight = 130;
    constexpr int kFaderWidth = 64;
}

RKRideEditor::RKRideEditor (RKRideProcessor& p)
    : rkfx::EditorBase (p, 900, 600),
      ride (p),
      graph (accent()),
      fader (accent()),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    target      = std::make_unique<Knob> (param ("target"), "Target");
    maxBoost    = std::make_unique<Knob> (param ("max_boost"), "Max Boost");
    maxCut      = std::make_unique<Knob> (param ("max_cut"), "Max Cut");
    speed       = std::make_unique<Knob> (param ("speed"), "Speed");
    floor       = std::make_unique<Knob> (param ("floor"), "Floor");
    returnAfter = std::make_unique<Knob> (param ("return"), "Return");
    above       = std::make_unique<Knob> (param ("above"), "Above Music");
    mode        = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent(), juce::StringArray {}, "Mode");

    addToCanvas (graph);
    addToCanvas (fader);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             target.get(), maxBoost.get(), maxCut.get(), speed.get(), floor.get(), returnAfter.get(), above.get(), mode.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
    meterTimer.startTimerHz (30);
}

void RKRideEditor::onParameterChange()
{
    const auto s = ride.currentSettings();
    above->setDimmed (! s.relative);
    graph.setSettings (s);
}

void RKRideEditor::updateMeters()
{
    bool fresh = false;
    const auto m = ride.takeMeter (fresh);
    if (! fresh)
        return;   // nothing processed: the graph stands still

    graph.push (m);
    fader.setGain (m.gainDb);
}

juce::Rectangle<int> RKRideEditor::graphSection() const
{
    return contentArea().withTrimmedBottom (kRiderHeight + kGap);
}

juce::Rectangle<int> RKRideEditor::riderSection() const
{
    return contentArea().removeFromBottom (kRiderHeight);
}

void RKRideEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = graphSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        area.removeFromBottom (8);
        fader.setBounds (area.removeFromRight (kFaderWidth));
        area.removeFromRight (kGap);
        graph.setBounds (area);
    }

    {
        auto area = riderSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto row = area.removeFromTop (Knob::kDefaultHeight);
        const auto width = row.getWidth() / 6;
        for (auto* k : { target.get(), maxBoost.get(), maxCut.get(), speed.get(), floor.get(), returnAfter.get() })
            k->setBounds (row.removeFromLeft (width));
    }

    auto global = globalArea();
    mode->setBounds (global.removeFromLeft (180).withSizeKeepingCentre (180, 44));
    global.removeFromLeft (12);
    above->setBounds (global.removeFromLeft (Knob::kDefaultWidth + 16).withSizeKeepingCentre (Knob::kDefaultWidth + 16, Knob::kDefaultHeight));
}

void RKRideEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, graphSection(), "Ride");
    drawSection (g, riderSection(), "Rider");
}
