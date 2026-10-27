#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kMeterWidth = 64;
    constexpr int kGateWidth = 300;
    constexpr int kTimingWidth = 230;
}

RKGateEditor::RKGateEditor (RKGateProcessor& p)
    : rkfx::EditorBase (p, 800, 420),
      gate (p),
      meter (accent(), 48.0f),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    threshold  = std::make_unique<Knob> (param ("threshold"), "Threshold");
    ratio      = std::make_unique<Knob> (param ("ratio"), "Ratio");
    range      = std::make_unique<Knob> (param ("range"), "Range");
    hysteresis = std::make_unique<Knob> (param ("hysteresis"), "Hysteresis");
    attack     = std::make_unique<Knob> (param ("attack"), "Attack");
    hold       = std::make_unique<Knob> (param ("hold"), "Hold");
    release    = std::make_unique<Knob> (param ("release"), "Release");
    keyHpf     = std::make_unique<Knob> (param ("key_hpf"), "High-Pass");
    keyLpf     = std::make_unique<Knob> (param ("key_lpf"), "Low-Pass");

    mode   = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent());
    source = std::make_unique<rkfx::SegmentedSwitch> (param ("sc_source"), accent(), juce::StringArray { "Int", "Ext" });
    listen = std::make_unique<rkfx::ToggleSwitch> (param ("key_listen"), rkfx::Palette::warning, "Listen");
    link   = std::make_unique<rkfx::ToggleSwitch> (param ("link"), accent(), "Link");

    addToCanvas (meter);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             threshold.get(), ratio.get(), range.get(), hysteresis.get(), attack.get(), hold.get(), release.get(),
             keyHpf.get(), keyLpf.get(), mode.get(), source.get(), listen.get(), link.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
    meterTimer.startTimerHz (30);
}

void RKGateEditor::onParameterChange()
{
    const auto s = gate.currentSettings();
    const auto expander = s.mode == GateEngine::Mode::expander;
    ratio->setDimmed (! expander);
    hysteresis->setDimmed (expander);
}

void RKGateEditor::updateMeter()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = lastMeterTime > 0.0 ? (float) (now - lastMeterTime) : 0.0f;
    lastMeterTime = now;
    meter.setReduction (gate.takeGainReductionDb(), elapsed);
}

juce::Rectangle<int> RKGateEditor::meterSection() const   { return contentArea().withWidth (kMeterWidth); }
juce::Rectangle<int> RKGateEditor::gateSection() const    { return contentArea().withTrimmedLeft (kMeterWidth + kGap).withWidth (kGateWidth); }
juce::Rectangle<int> RKGateEditor::timingSection() const  { return contentArea().withTrimmedLeft (kMeterWidth + kGateWidth + 2 * kGap).withWidth (kTimingWidth); }
juce::Rectangle<int> RKGateEditor::keySection() const     { return contentArea().withTrimmedLeft (kMeterWidth + kGateWidth + kTimingWidth + 3 * kGap); }

void RKGateEditor::layoutContent()
{
    using rkfx::Knob;

    meter.setBounds (meterSection().reduced (10, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (8));

    {
        auto area = gateSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        mode->setBounds (area.removeFromTop (26).withSizeKeepingCentre (180, 26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto quarter = knobs.getWidth() / 4;
        threshold->setBounds (knobs.removeFromLeft (quarter));
        ratio->setBounds (knobs.removeFromLeft (quarter));
        range->setBounds (knobs.removeFromLeft (quarter));
        hysteresis->setBounds (knobs);
    }

    {
        auto area = timingSection().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 34);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        attack->setBounds (knobs.removeFromLeft (third));
        hold->setBounds (knobs.removeFromLeft (third));
        release->setBounds (knobs);
    }

    {
        auto area = keySection().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        source->setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        keyHpf->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        keyLpf->setBounds (knobs);
        area.removeFromTop (8);
        auto toggles = area.removeFromTop (26);
        listen->setBounds (toggles.removeFromLeft (toggles.getWidth() / 2 - 3));
        toggles.removeFromLeft (6);
        link->setBounds (toggles);
    }
}

void RKGateEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, meterSection(), {});
    drawSection (g, gateSection(), "Gate");
    drawSection (g, timingSection(), "Timing");
    drawSection (g, keySection(), "Key");
}
