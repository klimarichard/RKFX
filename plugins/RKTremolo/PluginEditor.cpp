#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
}

RKTremoloEditor::RKTremoloEditor (RKTremoloProcessor& p)
    : rkfx::EditorBase (p, 640, 400),
      trem (p),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    mode     = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent());
    shape    = std::make_unique<rkfx::SegmentedSwitch> (param ("shape"), accent(),
                                                        juce::StringArray { "Sine", "Tri", "Sqr", "Up", "Down" });
    sync     = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    depth    = std::make_unique<Knob> (param ("depth"), "Depth");
    rate     = std::make_unique<Knob> (param ("rate"), "Rate");
    division = std::make_unique<Knob> (param ("division"), "Rate");
    smooth   = std::make_unique<Knob> (param ("smooth"), "Smoothing");
    phase    = std::make_unique<Knob> (param ("phase"), "Stereo Phase");
    xover    = std::make_unique<Knob> (param ("xover"), "Crossover");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             mode.get(), shape.get(), sync.get(), depth.get(), rate.get(), division.get(),
             smooth.get(), phase.get(), xover.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
}

void RKTremoloEditor::onParameterChange()
{
    const auto s = trem.currentSettings();
    rate->setVisible (! s.sync);
    division->setVisible (s.sync);
    xover->setDimmed (s.mode != TremoloEngine::Mode::harmonic);

    using Shape = rkfx::LFO::Shape;
    smooth->setDimmed (s.shape == Shape::sine || s.shape == Shape::triangle);
}

void RKTremoloEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = contentArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight);

    auto switches = area.removeFromTop (26);
    mode->setBounds (switches.removeFromLeft (170));
    switches.removeFromLeft (12);
    sync->setBounds (switches.removeFromRight (64));
    switches.removeFromRight (12);
    shape->setBounds (switches);

    area.removeFromTop (10);
    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto fifth = knobs.getWidth() / 5;
    depth->setBounds (knobs.removeFromLeft (fifth));
    rate->setBounds (knobs.removeFromLeft (fifth));
    division->setBounds (rate->getBounds());
    smooth->setBounds (knobs.removeFromLeft (fifth));
    phase->setBounds (knobs.removeFromLeft (fifth));
    xover->setBounds (knobs);
}

void RKTremoloEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Tremolo");
}
