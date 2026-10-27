#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKDoubleEditor::RKDoubleEditor (RKDoubleProcessor& p)
    : rkfx::EditorBase (p, 640, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    delay      = std::make_unique<Knob> (param ("delay"), "Delay");
    drift      = std::make_unique<Knob> (param ("drift"), "Drift");
    driftSpeed = std::make_unique<Knob> (param ("drift_speed"), "Drift Speed");
    tone       = std::make_unique<Knob> (param ("tone"), "Tone");
    spread     = std::make_unique<Knob> (param ("spread"), "Spread");
    level      = std::make_unique<Knob> (param ("level"), "Double Level");

    for (auto* k : { delay.get(), drift.get(), driftSpeed.get(), tone.get(), spread.get(), level.get() })
        addToCanvas (*k);

    // Drift speed does nothing while drift is 0.
    driftWatcher = std::make_unique<juce::ParameterAttachment> (param ("drift"), [this] (float v)
    {
        driftSpeed->setDimmed (v < 0.5f);
    });
    driftWatcher->sendInitialUpdate();

    finishSetup();
}

void RKDoubleEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight + 22);
    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto sixth = knobs.getWidth() / 6;
    for (auto* k : { delay.get(), drift.get(), driftSpeed.get(), tone.get(), spread.get() })
        k->setBounds (knobs.removeFromLeft (sixth));
    level->setBounds (knobs);
}

void RKDoubleEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Double");
}
