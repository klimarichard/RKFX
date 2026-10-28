#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKVibratoEditor::RKVibratoEditor (RKVibratoProcessor& p)
    : rkfx::EditorBase (p, 600, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    shape    = std::make_unique<rkfx::SegmentedSwitch> (param ("shape"), accent());
    sync     = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    rate     = std::make_unique<Knob> (param ("rate"), "Rate");
    division = std::make_unique<Knob> (param ("division"), "Rate");
    depth    = std::make_unique<Knob> (param ("depth"), "Depth");
    phase    = std::make_unique<Knob> (param ("phase"), "Stereo Phase");
    rise     = std::make_unique<Knob> (param ("rise"), "Rise");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             shape.get(), sync.get(), rate.get(), division.get(), depth.get(), phase.get(), rise.get() })
        addToCanvas (*c);

    syncWatcher = std::make_unique<juce::ParameterAttachment> (param ("sync"), [this] (float v)
    {
        division->setVisible (v >= 0.5f);
        rate->setVisible (v < 0.5f);
    });
    syncWatcher->sendInitialUpdate();

    finishSetup();
}

void RKVibratoEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight);
    auto switches = area.removeFromTop (26);
    shape->setBounds (switches.removeFromLeft (180));
    sync->setBounds (switches.removeFromRight (64));
    area.removeFromTop (12);

    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto quarter = knobs.getWidth() / 4;
    rate->setBounds (knobs.removeFromLeft (quarter));
    division->setBounds (rate->getBounds());
    depth->setBounds (knobs.removeFromLeft (quarter));
    phase->setBounds (knobs.removeFromLeft (quarter));
    rise->setBounds (knobs);
}

void RKVibratoEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Vibrato");
}
