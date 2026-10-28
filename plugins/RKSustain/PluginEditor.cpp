#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kModeHeight = kTitleHeight + 26 + 16;
}

RKSustainEditor::RKSustainEditor (RKSustainProcessor& p)
    : rkfx::EditorBase (p, 560, 400)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    mode  = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent(), juce::StringArray { "Sustain", "Sostenuto", "Hold", "Latch" });
    pedal = std::make_unique<rkfx::ToggleSwitch> (param ("pedal"), accent(), "Pedal");
    useCC = std::make_unique<rkfx::ToggleSwitch> (param ("use_cc"), accent(), "From MIDI");
    cc    = std::make_unique<Knob> (param ("cc"), "Controller");

    for (juce::Component* c : std::initializer_list<juce::Component*> { mode.get(), pedal.get(), useCC.get(), cc.get() })
        addToCanvas (*c);

    ccWatcher = std::make_unique<juce::ParameterAttachment> (param ("use_cc"), [this] (float v) { cc->setDimmed (v < 0.5f); });
    ccWatcher->sendInitialUpdate();

    finishSetup();
}

juce::Rectangle<int> RKSustainEditor::modeArea() const  { return contentArea().removeFromTop (kModeHeight); }
juce::Rectangle<int> RKSustainEditor::pedalArea() const { return contentArea().withTrimmedTop (kModeHeight + 10); }

void RKSustainEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = modeArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        mode->setBounds (area.removeFromTop (26).removeFromLeft (400));
    }

    {
        auto area = pedalArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto row = area.removeFromTop (Knob::kDefaultHeight);
        pedal->setBounds (row.removeFromLeft (110).withSizeKeepingCentre (110, 32));
        row.removeFromLeft (24);
        useCC->setBounds (row.removeFromLeft (110).withSizeKeepingCentre (110, 28));
        cc->setBounds (row.removeFromLeft (150));
    }
}

void RKSustainEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, modeArea(), "Mode");
    drawSection (g, pedalArea(), "Pedal");
}
