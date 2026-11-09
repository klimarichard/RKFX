#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"

using namespace MixGui::Layout;
using namespace mix;

//==============================================================================
// Patch page: which input each channel takes (docs/design/RKMix.md, Input patch)

namespace
{
    constexpr int kLightsHeight = 76;
    constexpr int kColumns = 4, kRows = 8;
    constexpr float kLightFloorDb = -60.0f;

    juce::Rectangle<int> innerOf (juce::Rectangle<int> a) { return a.withTrimmedTop (kSectionTitle).reduced (8, 2); }

    juce::String sourceName (int value) { return value == 0 ? juce::String ("Off") : "In " + juce::String (value); }
}

RKMixEditor::PatchPanel::PatchPanel (RKMixEditor& o) : Panel (o)
{
    lights.fill (kLightFloorDb);
    auto& s = o.mixer.getSettings();
    for (int ch = 0; ch < kChannels; ++ch)
    {
        auto& box = sources[(size_t) ch];
        box = std::make_unique<ChoiceBox> (s.param (channelId (ch, "src")), sourceName, 1);
        box->setTooltip ("The input channel " + num (ch) + " takes. One input can feed several channels");
        addAndMakeVisible (*box);
    }

    defaultButton.setTooltip ("Every channel back on its own input (channel 3 takes In 3)");
    defaultButton.onClick = [this]
    {
        if (! owner.mixer.isPatched())
            return;
        juce::Component::SafePointer<PatchPanel> safe (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Default patch",
                                            "Put every channel back on its own input?", "Default", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([safe] (int result)
                                            {
                                                if (safe != nullptr && result == 1)
                                                    safe->owner.mixer.setDefaultPatch();
                                            }));
    };
    addAndMakeVisible (defaultButton);

    swapButton.setClickingTogglesState (false);
    swapButton.setTooltip ("Swap two channels' inputs: click Swap, then the two channels (the usual fix for two swapped cables)");
    swapButton.onClick = [this]
    {
        swapping = ! swapping;
        swapFirst = -1;
        swapButton.setToggleState (swapping, juce::dontSendNotification);
        repaint();
    };
    addAndMakeVisible (swapButton);
}

RKMixEditor::PatchPanel::~PatchPanel()
{
    watch (false);
}

void RKMixEditor::PatchPanel::watch (bool on)
{
    if (on == watching)
        return;
    watching = on;
    owner.mixer.watchInputs (on);
    if (on)
    {
        lights.fill (kLightFloorDb);
        lightTimer.startTimerHz (15);
    }
    else
    {
        lightTimer.stopTimer();
    }
}

void RKMixEditor::PatchPanel::visibilityChanged()
{
    watch (isShowing());
    if (! isVisible() && swapping)
    {
        swapping = false;
        swapFirst = -1;
        swapButton.setToggleState (false, juce::dontSendNotification);
    }
}

void RKMixEditor::PatchPanel::updateLights()
{
    for (int in = 0; in < kChannels; ++in)
    {
        const auto peak = owner.mixer.takeInputPeak (in);
        const auto db = juce::Decibels::gainToDecibels (peak, kLightFloorDb);
        auto& light = lights[(size_t) in];
        light = juce::jmax (db, light - 3.0f);   // falls 45 dB a second
    }
    repaint (inputsArea);
}

void RKMixEditor::PatchPanel::refresh()
{
    users.fill (0);
    for (int ch = 0; ch < kChannels; ++ch)
    {
        const auto input = owner.mixer.getChannelInput (ch);
        if (input >= 0)
            ++users[(size_t) input];

        // Not the channel's own input: in the accent colour.
        auto& box = *sources[(size_t) ch];
        box.setColour (juce::ComboBox::textColourId, input != ch ? owner.accent().brighter (0.4f) : rkfx::Palette::text);
        box.setTooltip (input == ch ? "Channel " + num (ch) + " takes its own input, In " + num (ch)
                                    : input < 0 ? "Channel " + num (ch) + " is Off: silent"
                                                : "Channel " + num (ch) + " takes In " + num (input) + " (repatched)");
    }
    defaultButton.setEnabled (owner.mixer.isPatched());
    repaint();
}

juce::Rectangle<int> RKMixEditor::PatchPanel::lightArea (int input) const
{
    const auto a = innerOf (inputsArea);
    const auto w = a.getWidth() / kChannels;
    return { a.getX() + input * w, a.getY() + 4, w, a.getHeight() - 4 };
}

void RKMixEditor::PatchPanel::resized()
{
    auto r = getLocalBounds();
    inputsArea = r.removeFromTop (kLightsHeight);
    r.removeFromTop (kGap);
    channelsArea = r;

    auto buttons = innerOf (channelsArea).removeFromTop (26);
    swapButton.setBounds (buttons.removeFromRight (80));
    buttons.removeFromRight (6);
    defaultButton.setBounds (buttons.removeFromRight (80));

    const auto grid = innerOf (channelsArea).withTrimmedTop (32);
    const auto cellW = grid.getWidth() / kColumns, cellH = grid.getHeight() / kRows;
    for (int ch = 0; ch < kChannels; ++ch)
    {
        auto cell = juce::Rectangle<int> (grid.getX() + (ch / kRows) * cellW, grid.getY() + (ch % kRows) * cellH, cellW, cellH).reduced (3, 2);
        cells[(size_t) ch] = cell;
        sources[(size_t) ch]->setBounds (cell.removeFromRight (78).withSizeKeepingCentre (78, juce::jmin (24, cell.getHeight())));
    }
}

void RKMixEditor::PatchPanel::paint (juce::Graphics& g)
{
    // The inputs: a light for each, lit by its signal; an input no channel takes is drawn dim.
    rkfx::drawPanel (g, inputsArea, owner.panelColour(), "Inputs (signal as it arrives)");
    for (int in = 0; in < kChannels; ++in)
    {
        auto area = lightArea (in);
        const auto db = lights[(size_t) in];
        const auto lit = db > -50.0f;
        auto dot = area.removeFromTop (18).withSizeKeepingCentre (14, 14).toFloat();
        g.setColour (rkfx::Palette::knobTrack.darker (0.3f));
        g.fillEllipse (dot);
        if (lit)
        {
            const auto amount = juce::jlimit (0.25f, 1.0f, juce::jmap (db, -50.0f, -6.0f, 0.25f, 1.0f));
            g.setColour ((db > -1.0f ? rkfx::Palette::warning : owner.accent().brighter (0.5f)).withAlpha (amount));
            g.fillEllipse (dot.reduced (1.0f));
        }
        const auto used = users[(size_t) in] > 0;
        g.setColour (used ? rkfx::Palette::text : rkfx::Palette::knobTrack);
        g.setFont (rkfx::Fonts::bold (10.5f));
        g.drawText (juce::String (in + 1), area.removeFromTop (14), juce::Justification::centred);
        g.setFont (rkfx::Fonts::regular (9.5f));
        g.setColour (users[(size_t) in] > 1 ? owner.accent().brighter (0.4f) : rkfx::Palette::textDim);
        g.drawText (used ? (users[(size_t) in] > 1 ? "x" + juce::String (users[(size_t) in]) : juce::String()) : juce::String ("-"),
                    area.removeFromTop (12), juce::Justification::centred);
    }

    // The channels: number, colour, name, and its source box.
    rkfx::drawPanel (g, channelsArea, owner.panelColour(), "Channels");
    {
        g.setColour (swapping ? owner.accent().brighter (0.4f) : rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (12.0f));
        const auto hint = innerOf (channelsArea).removeFromTop (26).withRight (defaultButton.getX() - 8);
        g.drawText (swapping ? (swapFirst < 0 ? juce::String ("Swap: click the first channel")
                                              : "Swap: now the channel to swap with Ch " + num (swapFirst))
                             : juce::String ("One input can feed several channels. A key \"Ch n\" follows channel n's input. Talkback takes the input itself."),
                    hint, juce::Justification::centredLeft);
    }
    for (int ch = 0; ch < kChannels; ++ch)
    {
        auto cell = cells[(size_t) ch];
        if (ch == swapFirst)
        {
            g.setColour (owner.accent().withAlpha (0.25f));
            g.fillRoundedRectangle (cell.toFloat(), 3.0f);
        }
        const auto leader = owner.leaderOf (ch);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (11.5f));
        g.drawText (juce::String (ch + 1), cell.removeFromLeft (22), juce::Justification::centredRight);
        cell.removeFromLeft (4);
        g.setColour (MixGui::stripColour (owner.mixer.getChannelColour (leader)));
        g.fillRoundedRectangle (cell.removeFromLeft (4).reduced (0, 4).toFloat(), 2.0f);
        cell.removeFromLeft (5);
        auto name = owner.displayName (leader);
        if (owner.isLinked (ch / 2))
            name += ch % 2 == 0 ? " L" : " R";
        g.setColour (owner.mixer.getChannelInput (ch) != ch ? owner.accent().brighter (0.4f) : rkfx::Palette::text);
        g.setFont (rkfx::Fonts::regular (11.5f));
        g.drawFittedText (name, cell.withTrimmedRight (82), juce::Justification::centredLeft, 1);
    }
}

void RKMixEditor::PatchPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! swapping || owner.isLocked())
        return;
    for (int ch = 0; ch < kChannels; ++ch)
        if (cells[(size_t) ch].contains (e.getPosition()))
        {
            if (swapFirst < 0)
            {
                swapFirst = ch;
            }
            else if (ch != swapFirst)
            {
                const auto a = owner.mixer.getChannelInput (swapFirst), b = owner.mixer.getChannelInput (ch);
                owner.mixer.setChannelInput (swapFirst, b);
                owner.mixer.setChannelInput (ch, a);
                swapping = false;
                swapFirst = -1;
                swapButton.setToggleState (false, juce::dontSendNotification);
            }
            repaint();
            return;
        }
}
