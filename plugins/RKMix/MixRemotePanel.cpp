#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"
#include "MixQr.h"

using namespace MixGui::Layout;
using namespace mix;

// The Remote page: the monitor remote's settings, and a QR code per Aux bus
// for the musicians' phones.

namespace
{
    juce::Rectangle<int> inner (juce::Rectangle<int> a) { return a.withTrimmedTop (kSectionTitle).reduced (8, 2); }
    constexpr int kLinkWidth = 150, kLinkHeight = 180, kQrSize = 118;
}

RKMixEditor::RemotePanel::RemotePanel (RKMixEditor& o) : Panel (o)
{
    auto& remote = o.mixer.getRemote();
    const auto toggle = [this, &o] (juce::TextButton& b, const juce::String& tip)
    {
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonOnColourId, o.accent());
        b.setTooltip (tip);
        addAndMakeVisible (b);
    };
    toggle (onButton, "Serve the in-ear mixes to phones and tablets on this network");
    onButton.onClick = [this] { owner.mixer.getRemote().setEnabled (! owner.mixer.getRemote().isEnabled()); refresh(); };
    toggle (ipButton, "Links with this computer's IP address instead of its name (for phones that can't find .local names)");
    ipButton.onClick = [this] { owner.mixer.getRemote().setLinksUseIpAddress (! owner.mixer.getRemote().linksUseIpAddress()); refresh(); };
    newLinks.setTooltip ("Every bus gets a new link: phones with the old one must scan again");
    newLinks.onClick = [this]
    {
        juce::Component::SafePointer<RemotePanel> safe (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "New links",
                                            "Every bus gets a new link and QR code. Phones with the old link stop working "
                                            "and must scan again.", "New links", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([safe] (int result)
                                            {
                                                if (safe != nullptr && result == 1)
                                                {
                                                    safe->owner.mixer.getRemote().newLinks();
                                                    safe->refresh();
                                                }
                                            }));
    };
    addAndMakeVisible (newLinks);

    toggle (consoleOn, "Let iPads control the whole console (with the PIN)");
    consoleOn.onClick = [this] { auto& c = owner.mixer.getRemote().getConsole(); c.setEnabled (! c.isEnabled()); refresh(); };
    newPin.setTooltip ("A new random PIN: every iPad must type it again");
    newPin.onClick = [this] { owner.mixer.getRemote().getConsole().setPin (MixConsole::randomPin()); refresh(); };
    addAndMakeVisible (newPin);
    pin.setEditable (false, true, false);
    pin.setJustificationType (juce::Justification::centred);
    pin.setFont (rkfx::Fonts::bold (18.0f));
    pin.setColour (juce::Label::outlineColourId, rkfx::Palette::knobTrack);
    pin.setTooltip ("The console's PIN (double-click to change: 4 to 8 digits)");
    pin.onTextChange = [this]
    {
        if (! owner.mixer.getRemote().getConsole().setPin (pin.getText()))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "PIN", "A PIN is 4 to 8 digits.");
        refresh();
    };
    addAndMakeVisible (pin);

    port.setEditable (false, true, false);
    port.setJustificationType (juce::Justification::centred);
    port.setColour (juce::Label::outlineColourId, rkfx::Palette::knobTrack);
    port.setTooltip ("The port (double-click to change; 8080 unless another app uses it)");
    port.onTextChange = [this] { owner.mixer.getRemote().setPort (port.getText().getIntValue()); refresh(); };
    port.setText (juce::String (remote.getPort()), juce::dontSendNotification);
    addAndMakeVisible (port);
    refresh();
}

void RKMixEditor::RemotePanel::refresh()
{
    auto& remote = owner.mixer.getRemote();
    lastVersion = remote.getVersion();
    onButton.setToggleState (remote.isEnabled(), juce::dontSendNotification);
    onButton.setButtonText (remote.isEnabled() ? "Remote on" : "Remote off");
    ipButton.setToggleState (remote.linksUseIpAddress(), juce::dontSendNotification);
    if (! port.isBeingEdited())
        port.setText (juce::String (remote.getPort()), juce::dontSendNotification);
    auto& console = remote.getConsole();
    consoleOn.setToggleState (console.isEnabled(), juce::dontSendNotification);
    consoleOn.setButtonText (console.isEnabled() ? "Console on" : "Console off");
    if (! pin.isBeingEdited())
        pin.setText (console.getPin(), juce::dontSendNotification);

    // A link for each Aux bus in use (named, or going to an output), in a grid.
    links.clear();
    auto& s = owner.mixer.getSettings();
    for (int bus = 0; bus < kBuses; ++bus)
        if (owner.mixer.getBusType (bus) == BusType::aux
            && (owner.mixer.getBusName (bus).isNotEmpty() || s.choice (busId (bus, "out")) != kTargetOff))
            links.push_back ({ bus, remote.linkFor (bus), {} });
    lastConsoleVersion = -1;
    resized();
    updateConnections();
    repaint();
}

void RKMixEditor::RemotePanel::updateDevices()
{
    auto& console = owner.mixer.getRemote().getConsole();
    if (console.getVersion() == lastConsoleVersion)
        return;
    lastConsoleVersion = console.getVersion();
    devices = console.getDevices();
    disconnects.clear();
    for (const auto& d : devices)
    {
        auto b = std::make_unique<juce::TextButton> ("Disconnect");
        const auto id = d.id;
        b->setTooltip ("Let this iPad go: it must type the PIN again");
        b->onClick = [this, id] { owner.mixer.getRemote().getConsole().disconnect (id); };
        addAndMakeVisible (*b);
        disconnects.push_back (std::move (b));
    }
    resized();
    repaint();
}

void RKMixEditor::RemotePanel::updateConnections()
{
    auto& remote = owner.mixer.getRemote();
    if (remote.getVersion() != lastVersion)
        return refresh();
    updateDevices();
    const auto now = remote.connectedByBus();
    if (now != connected)
    {
        connected = now;
        repaint();
    }
}

void RKMixEditor::RemotePanel::resized()
{
    auto r = getLocalBounds();
    setupArea = r.removeFromLeft (250);
    r.removeFromLeft (kGap);
    consoleArea = r.removeFromLeft (250);
    r.removeFromLeft (kGap);
    linksArea = r;

    auto c = inner (consoleArea);
    auto top = c.removeFromTop (kSwitchH + 4);
    consoleOn.setBounds (top.removeFromLeft (110));
    top.removeFromLeft (6);
    newPin.setBounds (top);
    c.removeFromTop (8);
    auto pinRow = c.removeFromTop (30);
    pinRow.removeFromLeft (40);   // "PIN"
    pin.setBounds (pinRow.removeFromLeft (100));
    c.removeFromTop (6);
    consoleQr = c.removeFromTop (kQrSize).withSizeKeepingCentre (kQrSize, kQrSize);
    c.removeFromTop (22);   // the link
    for (size_t i = 0; i < disconnects.size(); ++i)
    {
        auto row = c.removeFromTop (24);
        c.removeFromTop (2);
        disconnects[i]->setBounds (row.removeFromRight (84));
        disconnects[i]->setVisible (row.getBottom() <= consoleArea.getBottom() - 4);
    }

    auto a = inner (setupArea);
    onButton.setBounds (a.removeFromTop (kSwitchH + 4));
    a.removeFromTop (8);
    auto row = a.removeFromTop (kSwitchH);
    row.removeFromLeft (40);   // "Port"
    port.setBounds (row.removeFromLeft (70));
    a.removeFromTop (8);
    ipButton.setBounds (a.removeFromTop (kSwitchH));
    a.removeFromTop (6);
    newLinks.setBounds (a.removeFromTop (kSwitchH));

    auto grid = inner (linksArea).withTrimmedTop (4);
    const auto perRow = juce::jmax (1, grid.getWidth() / kLinkWidth);
    for (size_t i = 0; i < links.size(); ++i)
        links[i].area = { grid.getX() + (int) (i % (size_t) perRow) * kLinkWidth, grid.getY() + (int) (i / (size_t) perRow) * kLinkHeight,
                          kLinkWidth, kLinkHeight };
}

void RKMixEditor::RemotePanel::paint (juce::Graphics& g)
{
    auto& remote = owner.mixer.getRemote();
    rkfx::drawPanel (g, setupArea, owner.panelColour(), "Monitor remote");
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawText ("Port", port.getBounds().withX (port.getX() - 40).withWidth (36), juce::Justification::centredLeft);

    auto text = inner (setupArea).withTop (newLinks.getBottom() + 10);
    const auto listening = remote.isListening();
    g.setColour (listening ? owner.accent().brighter (0.4f) : remote.isEnabled() ? rkfx::Palette::warning : rkfx::Palette::textDim);
    g.drawFittedText (remote.getStatus(), text.removeFromTop (34), juce::Justification::topLeft, 2);
    g.setColour (rkfx::Palette::textDim);
    g.drawFittedText ("Each musician scans their mix's code with the phone's camera and sets their own in-ears: the sends "
                      "into that mix and its level, nothing else. On an iPhone, Share > Add to Home Screen makes it an app. "
                      "The phone and this computer must be on the same network. In Gig Performer, put RKMix in the global "
                      "rackspace (only one RKMix can serve).",
                      text, juce::Justification::topLeft, 12);

    // The console remote
    {
        auto& console = remote.getConsole();
        rkfx::drawPanel (g, consoleArea, owner.panelColour(), "Console remote (iPad)");
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (12.0f));
        g.drawText ("PIN", pin.getBounds().withX (pin.getX() - 40).withWidth (36), juce::Justification::centredLeft);
        const auto link = remote.consoleLink();
        if (listening && console.isEnabled())
            MixQr (link).draw (g, consoleQr.toFloat());
        else
        {
            g.setColour (rkfx::Palette::knobTrack);
            g.drawRoundedRectangle (consoleQr.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.setColour (rkfx::Palette::textDim);
            g.setFont (rkfx::Fonts::regular (11.5f));
            g.drawFittedText (listening ? "Switch the console on" : "Switch the remote on", consoleQr.reduced (8), juce::Justification::centred, 2);
        }
        auto below = inner (consoleArea).withTop (consoleQr.getBottom() + 2);
        g.setFont (rkfx::Fonts::regular (11.0f));
        g.setColour (copied == link ? owner.accent().brighter (0.4f) : rkfx::Palette::textDim);
        g.drawFittedText (copied == link ? juce::String ("Link copied") : link, below.removeFromTop (18), juce::Justification::centred, 1, 0.7f);
        if (devices.empty())
        {
            g.setColour (rkfx::Palette::textDim);
            g.drawFittedText ("Scan the code with the iPad's camera, type the PIN, then Share > Add to Home Screen. "
                              "The whole console: faders, sends, every setting.", below.reduced (0, 4), juce::Justification::topLeft, 6);
        }
        for (size_t i = 0; i < devices.size() && i < disconnects.size(); ++i)
        {
            const auto& d = devices[i];
            const auto row = disconnects[i]->getBounds().withX (below.getX()).withRight (disconnects[i]->getX() - 4);
            g.setColour (d.signedIn ? owner.accent().brighter (0.4f) : rkfx::Palette::textDim);
            g.drawFittedText ((d.name.isNotEmpty() ? d.name : juce::String ("Device")) + " " + d.address
                              + (d.signedIn ? "" : " (PIN)"), row, juce::Justification::centredLeft, 1, 0.7f);
        }
    }

    rkfx::drawPanel (g, linksArea, owner.panelColour(), "In-ear mixes: one code per Aux bus (click a code to copy its link)");
    if (links.empty())
    {
        g.setColour (rkfx::Palette::textDim);
        g.drawText ("No in-ear mixes yet: on the Buses page, give an Aux bus a name or an output.", inner (linksArea),
                    juce::Justification::centred);
        return;
    }
    for (const auto& link : links)
    {
        auto cell = link.area.reduced (6, 2);
        const auto name = owner.busDisplayName (link.bus);
        g.setColour (MixGui::stripColour (owner.mixer.getBusColour (link.bus)));
        g.fillRoundedRectangle (cell.removeFromTop (4).toFloat(), 2.0f);
        g.setColour (rkfx::Palette::text);
        g.setFont (rkfx::Fonts::bold (13.0f));
        g.drawText (name, cell.removeFromTop (20), juce::Justification::centred, true);

        auto qrArea = cell.removeFromTop (kQrSize).withSizeKeepingCentre (kQrSize, kQrSize);
        if (listening)
            MixQr (link.url).draw (g, qrArea.toFloat());
        else
        {
            g.setColour (rkfx::Palette::knobTrack);
            g.drawRoundedRectangle (qrArea.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.setColour (rkfx::Palette::textDim);
            g.setFont (rkfx::Fonts::regular (11.5f));
            g.drawFittedText ("Switch the remote on", qrArea.reduced (8), juce::Justification::centred, 2);
        }

        g.setFont (rkfx::Fonts::regular (11.0f));
        const auto n = connected[(size_t) link.bus];
        g.setColour (n > 0 ? owner.accent().brighter (0.4f) : rkfx::Palette::textDim);
        g.drawText (copied == link.url ? juce::String ("Link copied") : n == 0 ? juce::String ("No phone")
                    : n == 1 ? juce::String ("1 phone") : juce::String (n) + " phones",
                    cell.removeFromTop (18), juce::Justification::centred);
    }
}

void RKMixEditor::RemotePanel::mouseDown (const juce::MouseEvent& e)
{
    if (consoleQr.contains (e.getPosition()))
    {
        copied = owner.mixer.getRemote().consoleLink();
        juce::SystemClipboard::copyTextToClipboard (copied);
        repaint();
        return;
    }
    for (const auto& link : links)
        if (link.area.contains (e.getPosition()))
        {
            juce::SystemClipboard::copyTextToClipboard (link.url);
            copied = link.url;
            repaint();
            return;
        }
}
