#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kLevelWidth = 240;
    constexpr int kStereoWidth = 340;
}

RKUtilityEditor::RKUtilityEditor (RKUtilityProcessor& p)
    : rkfx::EditorBase (p, 880, 470),
      utility (p),
      watcher (p, [this] { updateDelayInfo(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    gain        = std::make_unique<Knob> (param ("gain"), "Gain");
    pan         = std::make_unique<Knob> (param ("pan"), "Pan");
    width       = std::make_unique<Knob> (param ("width"), "Width");
    bassMono    = std::make_unique<Knob> (param ("bass_mono"), "Bass Mono");
    delayLeft   = std::make_unique<Knob> (param ("delay_left"), "Delay L");
    delayRight  = std::make_unique<Knob> (param ("delay_right"), "Delay R");
    mute        = std::make_unique<rkfx::ToggleSwitch> (param ("mute"), rkfx::Palette::warning, "Mute");
    swap        = std::make_unique<rkfx::ToggleSwitch> (param ("swap"), accent(), "Swap L/R");
    polLeft     = std::make_unique<rkfx::ToggleSwitch> (param ("pol_left"), accent(), juce::String::fromUTF8 ("\xc3\x98 L"));
    polRight    = std::make_unique<rkfx::ToggleSwitch> (param ("pol_right"), accent(), juce::String::fromUTF8 ("\xc3\x98 R"));
    channelMode = std::make_unique<rkfx::SegmentedSwitch> (param ("channel_mode"), accent(),
                                                           juce::StringArray { "Stereo", "L", "R", "M/S Enc", "M/S Dec" });

    delayInfo.setFont (rkfx::Fonts::regular (12.0f));
    delayInfo.setColour (juce::Label::textColourId, rkfx::Palette::textDim);
    delayInfo.setJustificationType (juce::Justification::centredLeft);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             gain.get(), pan.get(), width.get(), bassMono.get(), delayLeft.get(), delayRight.get(), mute.get(), swap.get(),
             polLeft.get(), polRight.get(), channelMode.get(), &delayInfo })
        addToCanvas (*c);

    updateDelayInfo();
    finishSetup();
}

void RKUtilityEditor::updateDelayInfo()
{
    // Each delay in ms, samples (at the current rate) and the distance sound travels in that time.
    const auto rate = utility.getSampleRate() > 0.0 ? utility.getSampleRate() : 48000.0;
    const auto s = utility.currentSettings();
    const auto describe = [rate] (const char* side, float ms)
    {
        return juce::String (side) + ": " + juce::String (ms, 2) + " ms = " + juce::String (ms * 0.001 * rate, 1)
             + " samples = " + juce::String (ms * UtilityEngine::kCmPerMs, 1) + " cm";
    };
    delayInfo.setText (describe ("L", s.delayLeftMs) + "\n" + describe ("R", s.delayRightMs), juce::dontSendNotification);
}

juce::Rectangle<int> RKUtilityEditor::levelArea() const  { return contentArea().withWidth (kLevelWidth); }
juce::Rectangle<int> RKUtilityEditor::stereoArea() const { return contentArea().withTrimmedLeft (kLevelWidth + kGap).withWidth (kStereoWidth); }
juce::Rectangle<int> RKUtilityEditor::alignArea() const  { return contentArea().withTrimmedLeft (kLevelWidth + kStereoWidth + 2 * kGap); }

void RKUtilityEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = levelArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 34);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        gain->setBounds (knobs.removeFromLeft (third));
        pan->setBounds (knobs.removeFromLeft (third));
        mute->setBounds (knobs.withSizeKeepingCentre (64, 30));
    }

    {
        auto area = stereoArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        channelMode->setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = knobs.getWidth() / 3;
        width->setBounds (knobs.removeFromLeft (third));
        bassMono->setBounds (knobs.removeFromLeft (third));
        swap->setBounds (knobs.withSizeKeepingCentre (84, 30));
    }

    {
        auto area = alignArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto toggles = area.removeFromTop (26);
        polLeft->setBounds (toggles.removeFromLeft (toggles.getWidth() / 2 - 3));
        toggles.removeFromLeft (6);
        polRight->setBounds (toggles);
        area.removeFromTop (8);
        auto knobs = area.removeFromTop (Knob::kDefaultHeight);
        delayLeft->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        delayRight->setBounds (knobs);
        area.removeFromTop (4);
        delayInfo.setBounds (area.removeFromTop (36));
    }
}

void RKUtilityEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, levelArea(), "Level");
    drawSection (g, stereoArea(), "Stereo");
    drawSection (g, alignArea(), "Polarity & Delay");
}
