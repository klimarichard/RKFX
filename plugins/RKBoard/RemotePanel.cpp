#include "RemotePanel.h"

#include "gui/LookAndFeel.h"
#include "host/remote/RemoteQr.h"

namespace rkboard
{

using namespace rkfx;

namespace
{
    void styleField (juce::Label& field, const juce::String& tooltip)
    {
        field.setEditable (true);
        field.setJustificationType (juce::Justification::centredLeft);
        field.setColour (juce::Label::backgroundColourId, Palette::background);
        field.setColour (juce::Label::outlineColourId, Palette::outline);
        field.setTooltip (tooltip);
    }

    void styleLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Fonts::regular (13.0f));
        label.setColour (juce::Label::textColourId, Palette::textDim);
        label.setJustificationType (juce::Justification::centredRight);
    }
}

RemotePanel::RemotePanel (BoardProcessor& b) : board (b), hub (b.getRemote())
{
    if (hub == nullptr)
        return;   // tests and tools: no remote

    onButton.onClick = [this] { hub->setEnabled (onButton.getToggleState()); refresh(); };
    ipButton.onClick = [this] { hub->setLinksUseIpAddress (ipButton.getToggleState()); refresh(); };
    ipButton.setTooltip ("The QR code and address use this computer's IP address instead of its name "
                         "(some networks don't resolve .local names; the IP can change).");
    addAndMakeVisible (onButton);
    addAndMakeVisible (ipButton);

    styleLabel (nameLabel, "This board");
    styleField (nameField, "This board's name in the remote's list (several RKBoards in one host: one per rackspace).");
    nameField.onTextChange = [this] { board.setRemoteName (nameField.getText()); refresh(); };
    styleLabel (portLabel, "Port");
    styleField (portField, "The port (1024 to 65535; RKMix's remote uses 8080).");
    portField.onTextChange = [this] { hub->setPort (portField.getText().getIntValue()); refresh(); };
    styleLabel (pinLabel, "PIN");
    styleField (pinField, "4 to 8 digits. A new PIN signs every device out.");
    pinField.onTextChange = [this] { hub->setPin (pinField.getText()); refresh(); };
    newPinButton.setTooltip ("A new random PIN: every device must sign in again.");
    newPinButton.onClick = [this]
    {
        juce::String pin;
        for (int i = 0; i < 4; ++i)
            pin << juce::Random::getSystemRandom().nextInt (10);
        hub->setPin (pin);
        refresh();
    };
    for (auto* c : std::initializer_list<juce::Component*> { &nameLabel, &nameField, &portLabel, &portField, &pinLabel, &pinField, &newPinButton })
        addAndMakeVisible (c);

    refresh();
    startTimerHz (4);
}

void RemotePanel::refresh()
{
    if (hub == nullptr)
        return;
    lastVersion = hub->getVersion();
    onButton.setToggleState (hub->isEnabled(), juce::dontSendNotification);
    ipButton.setToggleState (hub->linksUseIpAddress(), juce::dontSendNotification);
    if (! nameField.isBeingEdited())
        nameField.setText (board.getRemoteName(), juce::dontSendNotification);
    if (! portField.isBeingEdited())
        portField.setText (juce::String (hub->getPort()), juce::dontSendNotification);
    if (! pinField.isBeingEdited())
        pinField.setText (hub->getPin(), juce::dontSendNotification);

    // The devices, each with Disconnect.
    for (auto& d : devices)
        removeChildComponent (d.disconnect.get());
    devices.clear();
    for (const auto& d : hub->getDevices())
    {
        DeviceRow row;
        row.id = d.id;
        row.text = (d.name.isNotEmpty() ? d.name : juce::String ("A device")) + " (" + d.address + ")"
                 + (! d.signedIn ? juce::String ("  -  typing the PIN") : d.board.isNotEmpty() ? "  -  " + d.board : juce::String());
        row.disconnect = std::make_unique<juce::TextButton> ("Disconnect");
        row.disconnect->setTooltip ("Let this device go: it must type the PIN again.");
        const auto id = d.id;
        row.disconnect->onClick = [this, id] { hub->disconnect (id); refresh(); };
        addAndMakeVisible (*row.disconnect);
        devices.push_back (std::move (row));
    }
    resized();
    repaint();
}

void RemotePanel::timerCallback()
{
    if (hub != nullptr && hub->getVersion() != lastVersion)
        refresh();
}

void RemotePanel::resized()
{
    auto area = getLocalBounds().reduced (16).withTrimmedTop (30);
    qrArea = area.removeFromRight (240).removeFromTop (240);
    area.removeFromRight (20);

    auto row = [&area] { auto r = area.removeFromTop (28); area.removeFromTop (8); return r; };
    onButton.setBounds (row().removeFromLeft (200));
    statusArea = row();
    auto name = row();
    nameLabel.setBounds (name.removeFromLeft (80));
    nameField.setBounds (name.removeFromLeft (200).withTrimmedLeft (8));
    auto pin = row();
    pinLabel.setBounds (pin.removeFromLeft (80));
    pinField.setBounds (pin.removeFromLeft (110).withTrimmedLeft (8));
    newPinButton.setBounds (pin.removeFromLeft (90).withTrimmedLeft (8));
    auto port = row();
    portLabel.setBounds (port.removeFromLeft (80));
    portField.setBounds (port.removeFromLeft (110).withTrimmedLeft (8));
    ipButton.setBounds (row().removeFromLeft (360));

    area.removeFromTop (12);
    devicesArea = area;
    auto list = area.withTrimmedTop (24);
    for (auto& d : devices)
    {
        auto r = list.removeFromTop (28);
        list.removeFromTop (4);
        d.disconnect->setBounds (r.removeFromRight (96));
    }
}

void RemotePanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds(), Palette::panel, "Remote (phone and iPad)");

    if (hub == nullptr)
    {
        g.setColour (Palette::textDim);
        g.setFont (Fonts::regular (15.0f));
        g.drawText ("The remote runs in the plugin and the app.", getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setFont (Fonts::regular (13.0f));
    g.setColour (hub->isListening() ? Palette::text : Palette::textDim);
    g.drawFittedText (hub->getStatus(), statusArea, juce::Justification::centredLeft, 1, 0.8f);

    // The QR code: the address with the PIN, so scanning it signs in.
    if (hub->isListening())
    {
        RemoteQr (hub->link (true)).draw (g, qrArea.toFloat());
        g.setColour (Palette::textDim);
        g.drawFittedText (hub->link (false), qrArea.withY (qrArea.getBottom() + 4).withHeight (20), juce::Justification::centred, 1, 0.7f);
        g.drawFittedText ("Scan it with the phone's camera; Add to Home Screen for a full-screen app.",
                          qrArea.withY (qrArea.getBottom() + 24).withHeight (36), juce::Justification::centredTop, 2, 0.8f);
    }
    else
    {
        g.setColour (Palette::outline);
        g.drawRoundedRectangle (qrArea.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Palette::textDim);
        g.drawFittedText ("Switch the remote on", qrArea.reduced (8), juce::Justification::centred, 2);
    }

    g.setColour (Palette::textDim);
    g.drawText (devices.empty() ? "No devices connected" : "Connected", devicesArea.removeFromTop (20), juce::Justification::centredLeft);
    auto list = devicesArea.withTrimmedTop (4);
    g.setColour (Palette::text);
    for (const auto& d : devices)
    {
        auto r = list.removeFromTop (28);
        list.removeFromTop (4);
        g.drawFittedText (d.text, r.withTrimmedRight (104), juce::Justification::centredLeft, 1, 0.8f);
    }
}

} // namespace rkboard
