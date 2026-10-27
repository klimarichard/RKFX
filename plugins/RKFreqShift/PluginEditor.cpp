#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kStripGap = 8;
    constexpr int kTitleHeight = 26;
}

RKFreqShiftEditor::Strip::Strip (rkfx::ProcessorBase& p, int shift, juce::Colour accent)
{
    using FreqShiftParams::id;

    on       = std::make_unique<rkfx::ToggleSwitch> (*p.findParam (id (shift, "on")), accent, "On");
    range    = std::make_unique<rkfx::SegmentedSwitch> (*p.findParam (id (shift, "range")), accent);
    coarse   = std::make_unique<rkfx::Knob> (*p.findParam (id (shift, "freq")), "Frequency");
    fine     = std::make_unique<rkfx::Knob> (*p.findParam (id (shift, "fine")), "Frequency");
    updown   = std::make_unique<rkfx::Knob> (*p.findParam (id (shift, "updown")), "Up/Down");
    feedback = std::make_unique<rkfx::Knob> (*p.findParam (id (shift, "feedback")), "Feedback");
    level    = std::make_unique<rkfx::Knob> (*p.findParam (id (shift, "level")), "Level");

    rangeWatcher = std::make_unique<juce::ParameterAttachment> (*p.findParam (id (shift, "range")), [this] (float v)
    {
        const auto isFine = v < 0.5f;
        fine->setVisible (isFine);
        coarse->setVisible (! isFine);
    });

    onWatcher = std::make_unique<juce::ParameterAttachment> (*p.findParam (id (shift, "on")), [this] (float v)
    {
        const auto off = v < 0.5f;
        for (auto* k : { coarse.get(), fine.get(), updown.get(), feedback.get(), level.get() })
            k->setDimmed (off);
    });
}

RKFreqShiftEditor::RKFreqShiftEditor (RKFreqShiftProcessor& p)
    : rkfx::EditorBase (p, 680, 470)
{
    for (int s = 0; s < FreqShiftParams::kNumShifts; ++s)
    {
        auto strip = std::make_unique<Strip> (p, s, accent());
        for (juce::Component* c : { (juce::Component*) strip->on.get(), (juce::Component*) strip->range.get(),
                                    (juce::Component*) strip->coarse.get(), (juce::Component*) strip->fine.get(),
                                    (juce::Component*) strip->updown.get(), (juce::Component*) strip->feedback.get(),
                                    (juce::Component*) strip->level.get() })
            addToCanvas (*c);

        strip->rangeWatcher->sendInitialUpdate();
        strip->onWatcher->sendInitialUpdate();
        strips.push_back (std::move (strip));
    }

    spread  = std::make_unique<rkfx::Knob> (*p.findParam ("spread"), "Spread");
    lowCut  = std::make_unique<rkfx::Knob> (*p.findParam ("lowcut"), "Low Cut");
    highCut = std::make_unique<rkfx::Knob> (*p.findParam ("highcut"), "High Cut");
    addToCanvas (*spread);
    addToCanvas (*lowCut);
    addToCanvas (*highCut);

    finishSetup();
}

juce::Rectangle<int> RKFreqShiftEditor::stripArea (int shift) const
{
    const auto area = contentArea();
    const auto width = (area.getWidth() - kStripGap * (FreqShiftParams::kNumShifts - 1)) / FreqShiftParams::kNumShifts;
    return { area.getX() + shift * (width + kStripGap), area.getY(), width, area.getHeight() };
}

void RKFreqShiftEditor::layoutContent()
{
    using rkfx::Knob;

    for (int s = 0; s < (int) strips.size(); ++s)
    {
        auto& strip = *strips[(size_t) s];
        auto area = stripArea (s).reduced (8, 0);
        area.removeFromTop (kTitleHeight);

        auto switches = area.removeFromTop (26);
        strip.on->setBounds (switches.removeFromLeft (44));
        switches.removeFromLeft (6);
        strip.range->setBounds (switches);

        area.removeFromTop (6);
        const auto knobWidth = area.getWidth() / 2;

        auto row1 = area.removeFromTop (Knob::kDefaultHeight);
        strip.coarse->setBounds (row1.removeFromLeft (knobWidth));
        strip.fine->setBounds (strip.coarse->getBounds());
        strip.updown->setBounds (row1);

        auto row2 = area.removeFromTop (Knob::kDefaultHeight);
        strip.feedback->setBounds (row2.removeFromLeft (knobWidth));
        strip.level->setBounds (row2);
    }

    auto global = globalArea();
    for (auto* k : { spread.get(), lowCut.get(), highCut.get() })
    {
        k->setBounds (global.removeFromLeft (Knob::kDefaultWidth).withSizeKeepingCentre (Knob::kDefaultWidth, Knob::kDefaultHeight));
        global.removeFromLeft (rkfx::EditorBase::kMargin);
    }
}

void RKFreqShiftEditor::paintContent (juce::Graphics& g)
{
    for (int s = 0; s < FreqShiftParams::kNumShifts; ++s)
        drawSection (g, stripArea (s), "Shift " + juce::String (s + 1));
}
