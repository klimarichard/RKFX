#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kDelayWidth = 330;
    constexpr int kToneWidth = 176;
    constexpr int kModulationWidth = 174;
}

RKDelayEditor::RKDelayEditor (RKDelayProcessor& p)
    : rkfx::EditorBase (p, 910, 450)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    mode     = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent());
    stereo   = std::make_unique<rkfx::SegmentedSwitch> (param ("stereo"), accent());
    sync     = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    time     = std::make_unique<Knob> (param ("time"), "Time");
    division = std::make_unique<Knob> (param ("division"), "Time");
    feedback = std::make_unique<Knob> (param ("feedback"), "Feedback");
    offset   = std::make_unique<Knob> (param ("offset"), "R Offset");
    lowCut   = std::make_unique<Knob> (param ("lowcut"), "Low Cut");
    highCut  = std::make_unique<Knob> (param ("highcut"), "High Cut");
    modDepth = std::make_unique<Knob> (param ("mod_depth"), "Depth");
    modRate  = std::make_unique<Knob> (param ("mod_rate"), "Rate");
    duck     = std::make_unique<Knob> (param ("duck"), "Ducking");
    shimmer  = std::make_unique<Knob> (param ("shimmer"), "Shimmer");
    interval = std::make_unique<Knob> (param ("shimmer_interval"), "Interval");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             mode.get(), stereo.get(), sync.get(), time.get(), division.get(), feedback.get(), offset.get(),
             lowCut.get(), highCut.get(), modDepth.get(), modRate.get(), duck.get(), shimmer.get(), interval.get() })
        addToCanvas (*c);

    syncWatcher = std::make_unique<juce::ParameterAttachment> (param ("sync"), [this] (float v)
    {
        division->setVisible (v >= 0.5f);
        time->setVisible (v < 0.5f);
    });

    // The right channel's offset means nothing when both sides share one delay.
    stereoWatcher = std::make_unique<juce::ParameterAttachment> (param ("stereo"), [this] (float v)
    {
        offset->setDimmed (juce::roundToInt (v) == 0);
    });

    modWatcher = std::make_unique<juce::ParameterAttachment> (param ("mod_depth"), [this] (float v)
    {
        modRate->setDimmed (v < 0.5f);
    });

    shimmerWatcher = std::make_unique<juce::ParameterAttachment> (param ("shimmer"), [this] (float v)
    {
        interval->setDimmed (v <= 0.0f);
    });

    syncWatcher->sendInitialUpdate();
    shimmerWatcher->sendInitialUpdate();
    stereoWatcher->sendInitialUpdate();
    modWatcher->sendInitialUpdate();
    finishSetup();
}

juce::Rectangle<int> RKDelayEditor::delayArea() const      { return contentArea().withWidth (kDelayWidth); }
juce::Rectangle<int> RKDelayEditor::toneArea() const       { return contentArea().withTrimmedLeft (kDelayWidth + kGap).withWidth (kToneWidth); }
juce::Rectangle<int> RKDelayEditor::modulationArea() const { return contentArea().withTrimmedLeft (kDelayWidth + kToneWidth + 2 * kGap).withWidth (kModulationWidth); }
juce::Rectangle<int> RKDelayEditor::shimmerArea() const    { return contentArea().withTrimmedLeft (kDelayWidth + kToneWidth + kModulationWidth + 3 * kGap); }

void RKDelayEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = delayArea().reduced (8, 0);
        auto title = area.removeFromTop (kTitleHeight + 4);
        sync->setBounds (title.removeFromRight (60).withSizeKeepingCentre (60, 22));
        mode->setBounds (area.removeFromTop (26));
        area.removeFromTop (6);
        stereo->setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        time->setBounds (knobs.removeFromLeft (third));
        division->setBounds (time->getBounds());
        feedback->setBounds (knobs.removeFromLeft (third));
        offset->setBounds (knobs);
    }

    {
        auto area = toneArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 66);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        lowCut->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        highCut->setBounds (knobs);
    }

    {
        auto area = modulationArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 66);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        modDepth->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        modRate->setBounds (knobs);
    }

    {
        auto area = shimmerArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 66);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        shimmer->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        interval->setBounds (knobs);
    }

    duck->setBounds (globalArea().removeFromLeft (Knob::kDefaultWidth).withSizeKeepingCentre (Knob::kDefaultWidth, Knob::kDefaultHeight));
}

void RKDelayEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, delayArea(), "Delay");
    drawSection (g, toneArea(), "Feedback Tone");
    drawSection (g, modulationArea(), "Modulation");
    drawSection (g, shimmerArea(), "Shimmer");
}
