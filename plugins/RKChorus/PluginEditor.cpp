#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKChorusEditor::RKChorusEditor (RKChorusProcessor& p)
    : rkfx::EditorBase (p, 640, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    sync     = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    voices   = std::make_unique<Knob> (param ("voices"), "Voices");
    rate     = std::make_unique<Knob> (param ("rate"), "Rate");
    division = std::make_unique<Knob> (param ("division"), "Rate");
    depth    = std::make_unique<Knob> (param ("depth"), "Depth");
    delay    = std::make_unique<Knob> (param ("delay"), "Delay");
    width    = std::make_unique<Knob> (param ("width"), "Width");
    tone     = std::make_unique<Knob> (param ("tone"), "Tone");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             sync.get(), voices.get(), rate.get(), division.get(), depth.get(), delay.get(), width.get(), tone.get() })
        addToCanvas (*c);

    syncWatcher = std::make_unique<juce::ParameterAttachment> (param ("sync"), [this] (float v)
    {
        division->setVisible (v >= 0.5f);
        rate->setVisible (v < 0.5f);
    });
    syncWatcher->sendInitialUpdate();

    finishSetup();
}

void RKChorusEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    auto title = area.removeFromTop (kTitleHeight + 6);
    sync->setBounds (title.removeFromRight (64).withSizeKeepingCentre (64, 24));

    area.removeFromTop (16);
    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto sixth = knobs.getWidth() / 6;
    voices->setBounds (knobs.removeFromLeft (sixth));
    rate->setBounds (knobs.removeFromLeft (sixth));
    division->setBounds (rate->getBounds());
    depth->setBounds (knobs.removeFromLeft (sixth));
    delay->setBounds (knobs.removeFromLeft (sixth));
    width->setBounds (knobs.removeFromLeft (sixth));
    tone->setBounds (knobs);
}

void RKChorusEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Chorus");
}
