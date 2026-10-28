#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kReverbWidth = 290;
    constexpr int kToneWidth = 250;
}

RKVerbEditor::RKVerbEditor (RKVerbProcessor& p)
    : rkfx::EditorBase (p, 780, 450)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    type     = std::make_unique<rkfx::SegmentedSwitch> (param ("type"), accent());
    sync     = std::make_unique<rkfx::ToggleSwitch> (param ("predelay_sync"), accent(), "Sync");
    freeze   = std::make_unique<rkfx::ToggleSwitch> (param ("freeze"), accent(), "Freeze");
    decay    = std::make_unique<Knob> (param ("decay"), "Decay");
    size     = std::make_unique<Knob> (param ("size"), "Size");
    predelay = std::make_unique<Knob> (param ("predelay"), "Pre-Delay");
    division = std::make_unique<Knob> (param ("predelay_division"), "Pre-Delay");
    damping  = std::make_unique<Knob> (param ("damping"), "Damping");
    lowCut   = std::make_unique<Knob> (param ("lowcut"), "Low Cut");
    highCut  = std::make_unique<Knob> (param ("highcut"), "High Cut");
    mod      = std::make_unique<Knob> (param ("mod"), "Modulation");
    width    = std::make_unique<Knob> (param ("width"), "Width");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             type.get(), sync.get(), freeze.get(), decay.get(), size.get(), predelay.get(), division.get(),
             damping.get(), lowCut.get(), highCut.get(), mod.get(), width.get() })
        addToCanvas (*c);

    syncWatcher = std::make_unique<juce::ParameterAttachment> (param ("predelay_sync"), [this] (float v)
    {
        division->setVisible (v >= 0.5f);
        predelay->setVisible (v < 0.5f);
    });
    syncWatcher->sendInitialUpdate();

    finishSetup();
}

juce::Rectangle<int> RKVerbEditor::reverbArea() const { return contentArea().withWidth (kReverbWidth); }
juce::Rectangle<int> RKVerbEditor::toneArea() const   { return contentArea().withTrimmedLeft (kReverbWidth + kGap).withWidth (kToneWidth); }
juce::Rectangle<int> RKVerbEditor::spaceArea() const  { return contentArea().withTrimmedLeft (kReverbWidth + kToneWidth + 2 * kGap); }

void RKVerbEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = reverbArea().reduced (8, 0);
        auto title = area.removeFromTop (kTitleHeight + 4);
        sync->setBounds (title.removeFromRight (60).withSizeKeepingCentre (60, 22));
        type->setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        decay->setBounds (knobs.removeFromLeft (third));
        size->setBounds (knobs.removeFromLeft (third));
        predelay->setBounds (knobs);
        division->setBounds (knobs);
    }

    {
        auto area = toneArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 34);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        damping->setBounds (knobs.removeFromLeft (third));
        lowCut->setBounds (knobs.removeFromLeft (third));
        highCut->setBounds (knobs);
    }

    {
        auto area = spaceArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 34);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        mod->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        width->setBounds (knobs);
    }

    // Freeze sits on its own in the bottom row: easy to find, made for a footswitch.
    freeze->setBounds (globalArea().removeFromLeft (110).withSizeKeepingCentre (110, 40));
}

void RKVerbEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, reverbArea(), "Reverb");
    drawSection (g, toneArea(), "Tone");
    drawSection (g, spaceArea(), "Space");
}
