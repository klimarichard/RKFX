#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kMeterWidth = 64;
}

RKLimitEditor::RKLimitEditor (RKLimitProcessor& p)
    : rkfx::EditorBase (p, 640, 420),
      limiter (p),
      meter (accent())
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    input        = std::make_unique<Knob> (param ("input"), "Input");
    ceiling      = std::make_unique<Knob> (param ("ceiling"), "Ceiling");
    release      = std::make_unique<Knob> (param ("release"), "Release");
    link         = std::make_unique<Knob> (param ("link"), "Link");
    releaseAuto  = std::make_unique<rkfx::ToggleSwitch> (param ("release_auto"), accent(), "Auto");
    oversampling = std::make_unique<rkfx::SegmentedSwitch> (param ("oversampling"), accent(), juce::StringArray {}, "Oversampling");

    addToCanvas (meter);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             input.get(), ceiling.get(), release.get(), link.get(), releaseAuto.get(), oversampling.get() })
        addToCanvas (*c);

    autoWatcher = std::make_unique<juce::ParameterAttachment> (param ("release_auto"), [this] (float v)
    {
        release->setDimmed (v >= 0.5f);
    });
    autoWatcher->sendInitialUpdate();

    finishSetup();
    meterTimer.startTimerHz (30);
}

void RKLimitEditor::updateMeter()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = lastMeterTime > 0.0 ? (float) (now - lastMeterTime) : 0.0f;
    lastMeterTime = now;
    meter.setReduction (limiter.takeGainReductionDb(), elapsed);
}

juce::Rectangle<int> RKLimitEditor::meterArea() const   { return contentArea().withWidth (kMeterWidth); }
juce::Rectangle<int> RKLimitEditor::limiterArea() const { return contentArea().withTrimmedLeft (kMeterWidth + kGap); }

void RKLimitEditor::layoutContent()
{
    using rkfx::Knob;

    meter.setBounds (meterArea().reduced (10, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (8));

    auto area = limiterArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight + 30);
    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto fifth = knobs.getWidth() / 5;
    input->setBounds (knobs.removeFromLeft (fifth));
    ceiling->setBounds (knobs.removeFromLeft (fifth));
    release->setBounds (knobs.removeFromLeft (fifth));
    releaseAuto->setBounds (knobs.removeFromLeft (fifth).withSizeKeepingCentre (64, 28));
    link->setBounds (knobs);

    oversampling->setBounds (globalArea().removeFromLeft (180).withSizeKeepingCentre (180, 46));
}

void RKLimitEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, meterArea(), {});
    drawSection (g, limiterArea(), "Limiter");
}
