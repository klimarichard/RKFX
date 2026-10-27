#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kPhaserWidth = 220;
    constexpr int kLfoWidth = 270;
}

RKPhaseEditor::RKPhaseEditor (RKPhaseProcessor& p)
    : rkfx::EditorBase (p, 760, 450)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    stages     = std::make_unique<rkfx::SegmentedSwitch> (param ("stages"), accent(), juce::StringArray {}, "Stages");
    sync       = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    centre     = std::make_unique<Knob> (param ("center"), "Center");
    feedback   = std::make_unique<Knob> (param ("feedback"), "Feedback");
    depth      = std::make_unique<Knob> (param ("depth"), "Depth");
    rate       = std::make_unique<Knob> (param ("rate"), "Rate");
    division   = std::make_unique<Knob> (param ("division"), "Rate");
    phase      = std::make_unique<Knob> (param ("phase"), "Stereo Phase");
    envAmount  = std::make_unique<Knob> (param ("env_amount"), "Amount");
    envSens    = std::make_unique<Knob> (param ("env_sens"), "Sensitivity");
    envAttack  = std::make_unique<Knob> (param ("env_attack"), "Attack");
    envRelease = std::make_unique<Knob> (param ("env_release"), "Release");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             stages.get(), sync.get(), centre.get(), feedback.get(), depth.get(), rate.get(), division.get(),
             phase.get(), envAmount.get(), envSens.get(), envAttack.get(), envRelease.get() })
        addToCanvas (*c);

    syncWatcher = std::make_unique<juce::ParameterAttachment> (param ("sync"), [this] (float v)
    {
        division->setVisible (v >= 0.5f);
        rate->setVisible (v < 0.5f);
    });

    depthWatcher = std::make_unique<juce::ParameterAttachment> (param ("depth"), [this] (float v)
    {
        for (auto* k : { rate.get(), division.get(), phase.get() })
            k->setDimmed (v < 0.5f);
    });

    envWatcher = std::make_unique<juce::ParameterAttachment> (param ("env_amount"), [this] (float v)
    {
        for (auto* k : { envSens.get(), envAttack.get(), envRelease.get() })
            k->setDimmed (std::abs (v) < 0.5f);
    });

    syncWatcher->sendInitialUpdate();
    depthWatcher->sendInitialUpdate();
    envWatcher->sendInitialUpdate();
    finishSetup();
}

juce::Rectangle<int> RKPhaseEditor::phaserArea() const   { return contentArea().withWidth (kPhaserWidth); }
juce::Rectangle<int> RKPhaseEditor::lfoArea() const      { return contentArea().withTrimmedLeft (kPhaserWidth + kGap).withWidth (kLfoWidth); }
juce::Rectangle<int> RKPhaseEditor::envelopeArea() const { return contentArea().withTrimmedLeft (kPhaserWidth + kLfoWidth + 2 * kGap); }

void RKPhaseEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = phaserArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        stages->setBounds (area.removeFromTop (44));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        centre->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        feedback->setBounds (knobs);
    }

    {
        auto area = lfoArea().reduced (8, 0);
        auto title = area.removeFromTop (kTitleHeight + 4);
        sync->setBounds (title.removeFromRight (60).withSizeKeepingCentre (60, 22));
        area.removeFromTop (26);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        depth->setBounds (knobs.removeFromLeft (third));
        rate->setBounds (knobs.removeFromLeft (third));
        division->setBounds (rate->getBounds());
        phase->setBounds (knobs);
    }

    {
        auto area = envelopeArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        const auto half = area.getWidth() / 2;
        auto row1 = area.removeFromTop (Knob::kDefaultHeight);
        envAmount->setBounds (row1.removeFromLeft (half));
        envSens->setBounds (row1);
        auto row2 = area.removeFromTop (Knob::kDefaultHeight);
        envAttack->setBounds (row2.removeFromLeft (half));
        envRelease->setBounds (row2);
    }
}

void RKPhaseEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, phaserArea(), "Phaser");
    drawSection (g, lfoArea(), "LFO");
    drawSection (g, envelopeArea(), "Envelope");
}
