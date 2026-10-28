#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kMeterWidth = 64;
}

RKDeEssEditor::RKDeEssEditor (RKDeEssProcessor& p)
    : rkfx::EditorBase (p, 640, 420),
      deEsser (p),
      meter (accent())
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    detect       = std::make_unique<rkfx::SegmentedSwitch> (param ("detect"), accent());
    mode         = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent());
    freq         = std::make_unique<Knob> (param ("freq"), "Frequency");
    threshold    = std::make_unique<Knob> (param ("threshold"), "Threshold");
    thresholdRel = std::make_unique<Knob> (param ("threshold_rel"), "Threshold");
    range        = std::make_unique<Knob> (param ("range"), "Range");
    listen       = std::make_unique<rkfx::ToggleSwitch> (param ("listen"), rkfx::Palette::warning, "Listen");
    link         = std::make_unique<rkfx::ToggleSwitch> (param ("link"), accent(), "Link");

    addToCanvas (meter);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             detect.get(), mode.get(), freq.get(), threshold.get(), thresholdRel.get(), range.get(), listen.get(), link.get() })
        addToCanvas (*c);

    // The threshold knob follows the detection mode (each mode keeps its own value).
    detectWatcher = std::make_unique<juce::ParameterAttachment> (param ("detect"), [this] (float v)
    {
        const auto absolute = v >= 0.5f;
        threshold->setVisible (absolute);
        thresholdRel->setVisible (! absolute);
    });
    detectWatcher->sendInitialUpdate();

    finishSetup();
    meterTimer.startTimerHz (30);
}

void RKDeEssEditor::updateMeter()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = lastMeterTime > 0.0 ? (float) (now - lastMeterTime) : 0.0f;
    lastMeterTime = now;
    meter.setReduction (deEsser.takeGainReductionDb(), elapsed);
}

juce::Rectangle<int> RKDeEssEditor::meterArea() const { return contentArea().withWidth (kMeterWidth); }
juce::Rectangle<int> RKDeEssEditor::mainArea() const  { return contentArea().withTrimmedLeft (kMeterWidth + kGap); }

void RKDeEssEditor::layoutContent()
{
    using rkfx::Knob;

    meter.setBounds (meterArea().reduced (10, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (8));

    auto area = mainArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight);
    auto switches = area.removeFromTop (26);
    detect->setBounds (switches.removeFromLeft (180));
    switches.removeFromLeft (12);
    mode->setBounds (switches.removeFromLeft (190));
    area.removeFromTop (10);

    auto row = area.removeFromTop (Knob::kDefaultHeight);
    freq->setBounds (row.removeFromLeft (Knob::kDefaultWidth + 16));
    threshold->setBounds (row.removeFromLeft (Knob::kDefaultWidth + 16));
    thresholdRel->setBounds (threshold->getBounds());
    range->setBounds (row.removeFromLeft (Knob::kDefaultWidth + 16));
    row.removeFromLeft (16);
    auto toggles = row.withSizeKeepingCentre (row.getWidth(), 64);
    listen->setBounds (toggles.removeFromTop (28).withWidth (80));
    toggles.removeFromTop (8);
    link->setBounds (toggles.removeFromTop (28).withWidth (80));
}

void RKDeEssEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, meterArea(), {});
    drawSection (g, mainArea(), "De-Esser");
}
