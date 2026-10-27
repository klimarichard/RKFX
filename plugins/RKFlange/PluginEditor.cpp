#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKFlangeEditor::RKFlangeEditor (RKFlangeProcessor& p)
    : rkfx::EditorBase (p, 640, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    sync     = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    rate     = std::make_unique<Knob> (param ("rate"), "Rate");
    division = std::make_unique<Knob> (param ("division"), "Rate");
    depth    = std::make_unique<Knob> (param ("depth"), "Depth");
    manual   = std::make_unique<Knob> (param ("manual"), "Manual");
    feedback = std::make_unique<Knob> (param ("feedback"), "Feedback");
    phase    = std::make_unique<Knob> (param ("phase"), "Stereo Phase");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             sync.get(), rate.get(), division.get(), depth.get(), manual.get(), feedback.get(), phase.get() })
        addToCanvas (*c);

    syncWatcher = std::make_unique<juce::ParameterAttachment> (param ("sync"), [this] (float v)
    {
        division->setVisible (v >= 0.5f);
        rate->setVisible (v < 0.5f);
    });

    // With depth 0 the LFO does nothing: the flanger is a fixed comb filter.
    depthWatcher = std::make_unique<juce::ParameterAttachment> (param ("depth"), [this] (float v)
    {
        for (auto* k : { rate.get(), division.get(), phase.get() })
            k->setDimmed (v < 0.5f);
    });

    syncWatcher->sendInitialUpdate();
    depthWatcher->sendInitialUpdate();
    finishSetup();
}

void RKFlangeEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    auto title = area.removeFromTop (kTitleHeight + 6);
    sync->setBounds (title.removeFromRight (64).withSizeKeepingCentre (64, 24));

    area.removeFromTop (16);
    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto fifth = knobs.getWidth() / 5;
    rate->setBounds (knobs.removeFromLeft (fifth));
    division->setBounds (rate->getBounds());
    depth->setBounds (knobs.removeFromLeft (fifth));
    manual->setBounds (knobs.removeFromLeft (fifth));
    feedback->setBounds (knobs.removeFromLeft (fifth));
    phase->setBounds (knobs);
}

void RKFlangeEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Flanger");
}
