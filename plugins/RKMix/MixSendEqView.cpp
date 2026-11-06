#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"

using namespace MixGui::Layout;
using namespace mix;

// The send EQ on the console (docs/design/RKMix Send EQ.md): a small EQ
// button under each Aux and FX send on the Sends page, and above each fader
// while the faders show the sends to a bus. Click it for the EQ in a popover;
// right-click to copy it, paste it (onto the ganged channels too), or paste it
// onto every send to the same bus.

namespace
{
    bool sameSettings (const SendEqDsp::Settings& a, const SendEqDsp::Settings& b)
    {
        return a.on == b.on && juce::exactlyEqual (a.lowCutHz, b.lowCutHz)
            && juce::exactlyEqual (a.lowHz, b.lowHz) && juce::exactlyEqual (a.lowDb, b.lowDb)
            && juce::exactlyEqual (a.midHz, b.midHz) && juce::exactlyEqual (a.midDb, b.midDb) && juce::exactlyEqual (a.midQ, b.midQ)
            && juce::exactlyEqual (a.highHz, b.highHz) && juce::exactlyEqual (a.highDb, b.highDb);
    }
}

//==============================================================================
RKMixEditor::SendEqButton::SendEqButton (RKMixEditor& o, int ch, int b) : owner (o), channel (ch), bus (b)
{
    shown = owner.mixer.getSendEq (channel, bus);
    startTimerHz (5);
}

void RKMixEditor::SendEqButton::setTarget (int ch, int b)
{
    channel = ch;
    bus = b;
    shown = owner.mixer.getSendEq (channel, bus);
    setTooltip ("This channel's EQ in " + owner.busDisplayName (bus) + " only (click: edit, right-click: copy and paste)");
    repaint();
}

void RKMixEditor::SendEqButton::timerCallback()
{
    if (! isShowing())
        return;
    if (const auto now = owner.mixer.getSendEq (channel, bus); ! sameSettings (now, shown))
    {
        shown = now;
        repaint();
    }
}

void RKMixEditor::SendEqButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto on = shown.on;
    g.setColour (on ? owner.accent().withAlpha (0.85f) : rkfx::Palette::panel.brighter (0.08f));
    g.fillRoundedRectangle (r, 3.0f);

    auto area = getLocalBounds().reduced (3, 1);
    g.setColour (on ? juce::Colours::black : rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (11.0f));
    g.drawText ("EQ", area.removeFromLeft (18), juce::Justification::centredLeft);

    // Not flat: its curve, small (20 Hz to 20 kHz, +-15 dB).
    if (on && ! shown.isFlat() && area.getWidth() > 8)
    {
        const auto rate = owner.sampleRateForDisplay();
        const auto box = area.toFloat().reduced (1.0f, 1.0f);
        juce::Path p;
        constexpr int kPoints = 32;
        for (int i = 0; i <= kPoints; ++i)
        {
            const auto hz = 20.0 * std::pow (1000.0, (double) i / kPoints);
            const auto db = juce::jlimit (-15.0, 15.0, SendEqDsp::responseDb (shown, hz, rate));
            const auto x = box.getX() + box.getWidth() * (float) i / kPoints;
            const auto y = box.getCentreY() - (float) db / 15.0f * box.getHeight() * 0.5f;
            if (i == 0)
                p.startNewSubPath (x, y);
            else
                p.lineTo (x, y);
        }
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (1.3f));
    }
}

void RKMixEditor::SendEqButton::mouseDown (const juce::MouseEvent& e)
{
    if (owner.isLocked())
        return;
    if (e.mods.isPopupMenu())
        owner.showSendEqMenu (channel, bus, *this);
    else
        owner.showSendEq (channel, bus, *this);
}

//==============================================================================
struct RKMixEditor::SendEqPanel : public juce::Component, private juce::Timer
{
    SendEqPanel (RKMixEditor& o, int ch, int b) : owner (o), channel (ch), bus (b)
    {
        auto& s = owner.mixer.getSettings();
        const auto param = [&] (const char* field) -> juce::RangedAudioParameter& { return s.param (channelSendEqId (channel, bus, field)); };

        title = owner.displayName (channel) + " in " + owner.busDisplayName (bus) + ": send EQ";
        on = std::make_unique<rkfx::ToggleSwitch> (param ("on"), owner.accent(), "On");
        addAndMakeVisible (*on);
        flat.setTooltip ("Every band back to 0 dB and the low cut off (On stays)");
        flat.onClick = [param]
        {
            for (const auto* field : { "lc", "lsg", "mg", "hsg" })
            {
                auto& p = param (field);
                p.beginChangeGesture();
                p.setValueNotifyingHost (p.getDefaultValue());
                p.endChangeGesture();
            }
        };
        addAndMakeVisible (flat);

        const char* fields[] = { "lc", "lsf", "lsg", "mf", "mg", "mq", "hsf", "hsg" };
        const char* labels[] = { "Low Cut", "Low", "Gain", "Mid", "Gain", "Q", "High", "Gain" };
        for (int k = 0; k < 8; ++k)
        {
            knobs[(size_t) k] = std::make_unique<rkfx::Knob> (param (fields[k]), labels[k]);
            addAndMakeVisible (*knobs[(size_t) k]);
        }

        const auto rate = owner.sampleRateForDisplay();
        curve = std::make_unique<rkfx::ResponseCurve> ([this, rate] (double hz)
        {
            return SendEqDsp::responseDb (owner.mixer.getSendEq (channel, bus), hz, rate);
        }, kEqRangeDb, owner.accent());
        addAndMakeVisible (*curve);

        shown = owner.mixer.getSendEq (channel, bus);
        setSize (8 * rkfx::Knob::kDefaultWidth + 16, 30 + 120 + 8 + rkfx::Knob::kDefaultHeight + 8);
        startTimerHz (15);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 4);
        auto top = r.removeFromTop (26);
        flat.setBounds (top.removeFromRight (60).reduced (0, 2));
        top.removeFromRight (6);
        on->setBounds (top.removeFromRight (60).reduced (0, 2));
        curve->setBounds (r.removeFromTop (120));
        r.removeFromTop (8);
        for (auto& k : knobs)
            k->setBounds (r.removeFromLeft (rkfx::Knob::kDefaultWidth));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (rkfx::Palette::text);
        g.setFont (rkfx::Fonts::bold (14.0f));
        g.drawText (title, getLocalBounds().reduced (8, 4).removeFromTop (26), juce::Justification::centredLeft);
    }

    void timerCallback() override
    {
        // Locked meanwhile: the popover goes.
        if (owner.isLocked())
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
            return;
        }
        if (const auto now = owner.mixer.getSendEq (channel, bus); ! sameSettings (now, shown))
        {
            shown = now;
            curve->refresh();
        }
    }

    RKMixEditor& owner;
    const int channel, bus;
    juce::String title;
    std::unique_ptr<rkfx::ToggleSwitch> on;
    juce::TextButton flat { "Flat" };
    std::array<std::unique_ptr<rkfx::Knob>, 8> knobs;
    std::unique_ptr<rkfx::ResponseCurve> curve;
    SendEqDsp::Settings shown;
};

//==============================================================================
void RKMixEditor::showSendEq (int channel, int bus, juce::Component& from)
{
    closeSendEq();
    if (isLocked() || ! mixer.hasSendEq (bus))
        return;
    auto panel = std::make_unique<SendEqPanel> (*this, leaderOf (channel), bus);
    sendEqBox = &juce::CallOutBox::launchAsynchronously (std::move (panel), getLocalArea (&from, from.getLocalBounds()), this);
}

void RKMixEditor::closeSendEq()
{
    if (sendEqBox != nullptr)
        sendEqBox->dismiss();
}

void RKMixEditor::showSendEqMenu (int channel, int bus, juce::Component& from)
{
    if (isLocked())
        return;
    channel = leaderOf (channel);
    const auto fits = RKMixProcessor::isSendEqCopy (clipboard);
    auto pasteText = juce::String ("Paste");
    if (fits)
    {
        pasteText << " " << RKMixProcessor::describeCopy (clipboard);
        if (const auto n = pasteTargets (channel).size(); n > 1)
            pasteText << " to " << n << " ganged channels";
    }

    juce::PopupMenu menu;
    menu.addItem (1, "Copy send EQ");
    menu.addItem (2, pasteText, fits);
    menu.addItem (3, "Paste to all sends to " + busDisplayName (bus), fits);

    juce::Component::SafePointer<RKMixEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&from), [safe, channel, bus] (int result)
    {
        if (safe == nullptr || result <= 0 || safe->isLocked())
            return;
        auto& m = safe->mixer;
        if (result == 1)
            safe->clipboard = m.copySendEq (channel, bus);
        else if (result == 2)
            for (const auto ch : safe->pasteTargets (channel))
                m.pasteSendEq (ch, bus, safe->clipboard);
        else if (result == 3)
            m.pasteSendEqToBus (bus, safe->clipboard);
    });
}
