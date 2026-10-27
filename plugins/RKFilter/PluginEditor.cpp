#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kSwitchHeight = 26;
    constexpr int kFilterWidth = 200;
    constexpr int kEnvelopeWidth = 176;
}

RKFilterEditor::RKFilterEditor (RKFilterProcessor& p)
    : rkfx::EditorBase (p, 760, 450)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    mode     = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent(), juce::StringArray { "LP", "HP", "BP" });
    slope    = std::make_unique<rkfx::SegmentedSwitch> (param ("slope"), accent(), juce::StringArray { "12", "24", "36", "48" });
    lfoShape = std::make_unique<rkfx::SegmentedSwitch> (param ("lfo_shape"), accent(),
                                                        juce::StringArray { "Sine", "Tri", "Sqr", "Up", "Down", "S&H" });
    lfoSync  = std::make_unique<rkfx::ToggleSwitch> (param ("lfo_sync"), accent(), "Sync");

    cutoff      = std::make_unique<Knob> (param ("cutoff"), "Cutoff");
    resonance   = std::make_unique<Knob> (param ("resonance"), "Resonance");
    envAmount   = std::make_unique<Knob> (param ("env_amount"), "Amount");
    envSens     = std::make_unique<Knob> (param ("env_sens"), "Sensitivity");
    envAttack   = std::make_unique<Knob> (param ("env_attack"), "Attack");
    envRelease  = std::make_unique<Knob> (param ("env_release"), "Release");
    lfoDepth    = std::make_unique<Knob> (param ("lfo_depth"), "Depth");
    lfoRate     = std::make_unique<Knob> (param ("lfo_rate"), "Rate");
    lfoDivision = std::make_unique<Knob> (param ("lfo_division"), "Rate");
    lfoPhase    = std::make_unique<Knob> (param ("lfo_phase"), "Stereo Phase");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             mode.get(), slope.get(), lfoShape.get(), lfoSync.get(), cutoff.get(), resonance.get(),
             envAmount.get(), envSens.get(), envAttack.get(), envRelease.get(),
             lfoDepth.get(), lfoRate.get(), lfoDivision.get(), lfoPhase.get() })
        addToCanvas (*c);

    syncWatcher = std::make_unique<juce::ParameterAttachment> (param ("lfo_sync"), [this] (float v)
    {
        lfoDivision->setVisible (v >= 0.5f);
        lfoRate->setVisible (v < 0.5f);
    });

    envWatcher = std::make_unique<juce::ParameterAttachment> (param ("env_amount"), [this] (float v)
    {
        for (auto* k : { envSens.get(), envAttack.get(), envRelease.get() })
            k->setDimmed (std::abs (v) < 0.5f);
    });

    lfoWatcher = std::make_unique<juce::ParameterAttachment> (param ("lfo_depth"), [this] (float v)
    {
        for (auto* k : { lfoRate.get(), lfoDivision.get(), lfoPhase.get() })
            k->setDimmed (std::abs (v) < 0.5f);
    });

    syncWatcher->sendInitialUpdate();
    envWatcher->sendInitialUpdate();
    lfoWatcher->sendInitialUpdate();

    finishSetup();
}

juce::Rectangle<int> RKFilterEditor::filterArea() const
{
    return contentArea().withWidth (kFilterWidth);
}

juce::Rectangle<int> RKFilterEditor::envelopeArea() const
{
    return contentArea().withTrimmedLeft (kFilterWidth + kGap).withWidth (kEnvelopeWidth);
}

juce::Rectangle<int> RKFilterEditor::lfoArea() const
{
    return contentArea().withTrimmedLeft (kFilterWidth + kEnvelopeWidth + 2 * kGap);
}

void RKFilterEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = filterArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        mode->setBounds (area.removeFromTop (kSwitchHeight));
        area.removeFromTop (6);
        slope->setBounds (area.removeFromTop (kSwitchHeight));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        cutoff->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        resonance->setBounds (knobs);
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

    {
        auto area = lfoArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        lfoShape->setBounds (area.removeFromTop (kSwitchHeight));
        area.removeFromTop (6);
        lfoSync->setBounds (area.removeFromTop (kSwitchHeight).removeFromLeft (64));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        lfoDepth->setBounds (knobs.removeFromLeft (third));
        lfoRate->setBounds (knobs.removeFromLeft (third));
        lfoDivision->setBounds (lfoRate->getBounds());
        lfoPhase->setBounds (knobs);
    }
}

void RKFilterEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, filterArea(), "Filter");
    drawSection (g, envelopeArea(), "Envelope");
    drawSection (g, lfoArea(), "LFO");
}
