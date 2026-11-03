#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"

using namespace MixGui::Layout;
using namespace mix;

// The DCAs and mute groups: their strips on the DCA page, their members
// pages, and spilling a DCA's members onto the faders.

namespace
{
    juce::Rectangle<int> inner (juce::Rectangle<int> a) { return a.withTrimmedTop (kSectionTitle).reduced (8, 2); }
}

//==============================================================================
RKMixEditor::GroupStrip::GroupStrip (RKMixEditor& o, int i, bool isMuteGroup)
    : owner (o), index (i), muteGroup (isMuteGroup)
{
    styleName (name);
    name.setTooltip ("Click for its members, double-click to rename" + juce::String (muteGroup ? "" : ", right-click for its colour"));
    name.onTextChange = [this]
    {
        if (muteGroup)
            owner.mixer.setMuteGroupName (index, name.getText());
        else
            owner.mixer.setDcaName (index, name.getText());
    };
    name.addMouseListener (this, false);
    addAndMakeVisible (name);

    if (muteGroup)
    {
        mute = std::make_unique<rkfx::ToggleSwitch> (*o.mixer.findParam (muteGroupId (index)), rkfx::Palette::warning, "MUTE");
    }
    else
    {
        mute = std::make_unique<rkfx::ToggleSwitch> (*o.mixer.findParam (dcaMuteId (index)), rkfx::Palette::warning, "Mute");
        fader = std::make_unique<MixGui::Fader> (*o.mixer.findParam (dcaFaderId (index)));
        addAndMakeVisible (*fader);
        spill.setClickingTogglesState (false);
        spill.setTooltip ("Show this DCA's channels and buses on the faders");
        spill.onClick = [this] { owner.setSpill (owner.spillDca == index ? -1 : index); };
        addAndMakeVisible (spill);
    }
    addAndMakeVisible (*mute);
    refresh();
}

void RKMixEditor::GroupStrip::refresh()
{
    const auto text = muteGroup ? owner.mixer.getMuteGroupName (index) : owner.mixer.getDcaName (index);
    if (! name.isBeingEdited())
        name.setText (text.isNotEmpty() ? text : muteGroup ? owner.muteGroupDisplayName (index) : owner.dcaDisplayName (index),
                      juce::dontSendNotification);

    members.clearQuick();
    for (const auto m : muteGroup ? owner.mixer.getMuteGroupMembers (index) : owner.mixer.getDcaMembers (index))
        members.add (owner.stripDisplayName (m));
    spill.setToggleState (owner.spillDca == index, juce::dontSendNotification);
    spill.setEnabled (! members.isEmpty());
    repaint();
}

void RKMixEditor::GroupStrip::resized()
{
    auto r = getLocalBounds().reduced (3, 0);
    r.removeFromTop (kColourBar + 2);
    name.setBounds (r.removeFromTop (kStripNameHeight));
    r.removeFromTop (kStripNumberHeight);
    r.removeFromTop (4);

    if (muteGroup)
    {
        mute->setBounds (r.removeFromTop (64).reduced (2, 0));
        r.removeFromTop (8);
        membersArea = r.reduced (2, 0);
        return;
    }

    spill.setBounds (r.removeFromTop (kButtonHeight).reduced (2, 0));
    r.removeFromTop (2);
    membersArea = r.removeFromTop (kSectionRowHeight * 3);
    r.removeFromTop (4);
    mute->setBounds (r.removeFromTop (kButtonHeight).reduced (2, 0));
    r.removeFromTop (6);
    fader->setBounds (r.withTrimmedRight (kMeterWidth + 2));
}

void RKMixEditor::GroupStrip::paint (juce::Graphics& g)
{
    const auto isSelected = muteGroup ? owner.selectedMuteGroup == index : owner.selectedDca == index;
    g.setColour (isSelected ? owner.accent().withAlpha (0.16f) : owner.panelColour());
    g.fillRoundedRectangle (getLocalBounds().toFloat(), rkfx::Style::kPanelRadius);
    if (isSelected)
    {
        g.setColour (owner.accent());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), rkfx::Style::kPanelRadius, 1.0f);
    }

    g.setColour (muteGroup ? rkfx::Palette::warning.withAlpha (0.7f) : MixGui::stripColour (owner.mixer.getDcaColour (index)));
    g.fillRoundedRectangle (getLocalBounds().removeFromTop (kColourBar).reduced (4, 0).toFloat().withY (2.0f), 2.0f);

    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (11.0f));
    g.drawText ((muteGroup ? "Mute " : "DCA ") + num (index), 0, kColourBar + 2 + kStripNameHeight, getWidth(), kStripNumberHeight,
                juce::Justification::centred);

    // The members: names on a mute group, how many on a DCA.
    g.setFont (rkfx::Fonts::regular (10.5f));
    if (! muteGroup)
    {
        g.drawFittedText (members.isEmpty() ? juce::String ("no members")
                                            : juce::String (members.size()) + (members.size() == 1 ? " member" : " members"),
                          membersArea, juce::Justification::centred, 2);
        return;
    }
    if (members.isEmpty())
    {
        g.drawFittedText ("No members: click the name to choose them", membersArea, juce::Justification::centredTop, 4);
        return;
    }
    auto list = membersArea;
    const auto rows = juce::jmax (1, list.getHeight() / 14);
    for (int i = 0; i < members.size() && i < rows; ++i)
    {
        const auto more = i == rows - 1 && members.size() > rows;
        g.drawText (more ? "+ " + juce::String (members.size() - i) + " more" : members[i], list.removeFromTop (14),
                    juce::Justification::centred, true);
    }
}

void RKMixEditor::GroupStrip::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && owner.isLocked())
        return;
    if (e.mods.isPopupMenu() && ! muteGroup)
    {
        juce::PopupMenu menu;
        const auto current = owner.mixer.getDcaColour (index);
        for (int i = 0; i < MixGui::kNumStripColours; ++i)
            menu.addColouredItem (i + 1, MixGui::stripColourName (i), MixGui::stripColour (i), true, current == i);
        juce::Component::SafePointer<RKMixEditor> safe (&owner);
        const auto dca = index;
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, dca] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->mixer.setDcaColour (dca, result - 1);
        });
        return;
    }
    owner.selectGroup (index, muteGroup);
}

//==============================================================================
RKMixEditor::GroupPanel::GroupPanel (RKMixEditor& o, int i, bool isMuteGroup) : Panel (o), index (i), muteGroup (isMuteGroup)
{
    auto& s = o.mixer.getSettings();
    styleName (name, 16.0f);
    name.setJustificationType (juce::Justification::centredLeft);
    name.setTooltip ("Double-click to rename");
    name.onTextChange = [this]
    {
        if (muteGroup)
            owner.mixer.setMuteGroupName (index, name.getText());
        else
            owner.mixer.setDcaName (index, name.getText());
    };
    addAndMakeVisible (name);

    const auto colour = muteGroup ? rkfx::Palette::warning : o.accent();
    for (int ch = 0; ch < kChannels; ++ch)
        channels[(size_t) ch] = make<rkfx::ToggleSwitch> (s.param (muteGroup ? channelMuteGroupId (ch, i) : channelDcaId (ch, i)),
                                                          colour, num (ch));
    for (int bus = 0; bus < kBuses; ++bus)
        buses[(size_t) bus] = make<rkfx::ToggleSwitch> (s.param (muteGroup ? busMuteGroupId (bus, i) : busDcaId (bus, i)),
                                                        colour, "B" + num (bus));

    if (! muteGroup)
    {
        spill.setClickingTogglesState (false);
        spill.setTooltip ("Show this DCA's channels and buses on the faders");
        spill.onClick = [this] { owner.setSpill (owner.spillDca == index ? -1 : index); };
        addAndMakeVisible (spill);
    }
    clear.setTooltip ("Take every channel and bus out");
    clear.onClick = [this]
    {
        auto& settings = owner.mixer.getSettings();
        for (int ch = 0; ch < kChannels; ++ch)
            settings.param (muteGroup ? channelMuteGroupId (ch, index) : channelDcaId (ch, index)).setValueNotifyingHost (0.0f);
        for (int bus = 0; bus < kBuses; ++bus)
            settings.param (muteGroup ? busMuteGroupId (bus, index) : busDcaId (bus, index)).setValueNotifyingHost (0.0f);
    };
    addAndMakeVisible (clear);
    refresh();
}

void RKMixEditor::GroupPanel::refresh()
{
    const auto text = muteGroup ? owner.mixer.getMuteGroupName (index) : owner.mixer.getDcaName (index);
    if (! name.isBeingEdited())
        name.setText (text, juce::dontSendNotification);
    // A linked pair is one member: its left channel.
    for (int ch = 0; ch < kChannels; ++ch)
        channels[(size_t) ch]->setVisible (owner.leaderOf (ch) == ch);
    spill.setToggleState (owner.spillDca == index, juce::dontSendNotification);
    const auto members = muteGroup ? owner.mixer.getMuteGroupMembers (index) : owner.mixer.getDcaMembers (index);
    spill.setEnabled (! members.isEmpty());
    clear.setEnabled (! members.isEmpty());
    repaint();
}

void RKMixEditor::GroupPanel::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kPanelHeader).reduced (4, 2);
    header.removeFromLeft (64);
    clear.setBounds (header.removeFromRight (80));
    header.removeFromRight (8);
    if (! muteGroup)
    {
        spill.setBounds (header.removeFromRight (80));
        header.removeFromRight (12);
        placeSwatches (header, swatches);
        header.removeFromRight (12);
    }
    name.setBounds (header.withWidth (juce::jmin (header.getWidth(), 260)));
    r.removeFromTop (4);

    infoArea = r.removeFromLeft (220);
    r.removeFromLeft (kGap);
    membersArea = r;

    // Two rows of 16 inputs, then the 16 buses; each name below its switch.
    auto a = inner (membersArea).withTrimmedTop (20);
    const auto w = (a.getWidth() - 15 * 4) / 16;
    constexpr int kRow = kSwitchH + 22;
    for (int ch = 0; ch < kChannels; ++ch)
        channels[(size_t) ch]->setBounds (a.getX() + (ch % 16) * (w + 4), a.getY() + (ch / 16) * kRow, w, kSwitchH);
    const auto busTop = a.getY() + 2 * kRow + 28;
    for (int bus = 0; bus < kBuses; ++bus)
        buses[(size_t) bus]->setBounds (a.getX() + bus * (w + 4), busTop, w, kSwitchH);
}

void RKMixEditor::GroupPanel::paint (juce::Graphics& g)
{
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (14.0f));
    g.drawText ((muteGroup ? "Mute " : "DCA ") + num (index), 4, 2, 64, 26, juce::Justification::centredLeft);
    if (name.getText().isEmpty() && ! name.isBeingEdited())
    {
        g.setFont (rkfx::Fonts::regular (14.0f));
        g.drawText ("Double-click to name it", name.getBounds().withTrimmedLeft (4), juce::Justification::centredLeft);
    }
    if (! muteGroup)
        paintSwatches (g, swatches, owner.mixer.getDcaColour (index), MixGui::stripColour);

    rkfx::drawPanel (g, infoArea, owner.panelColour(), muteGroup ? "Mute group" : "DCA");
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.drawFittedText (muteGroup
                          ? "Its MUTE (on the DCA page, or the host's Mute Group parameter) silences every member at once, "
                            "in the main and in every send, the in-ears too. The members' own Mute buttons stay as they are: "
                            "when the group is off again, they play as before. A muted member shows its number on orange."
                          : "Its fader moves every member's fader by its own level: -6 dB on the DCA is -6 dB on each of them, "
                            "their own faders stay where they are. Its bottom silences them, its Mute mutes them. It sums "
                            "nothing and adds no processing: pre-fader sends (the in-ears) don't change. Spill shows the "
                            "members on the faders.",
                      inner (infoArea), juce::Justification::topLeft, 14);

    rkfx::drawPanel (g, membersArea, owner.panelColour(), "Members");
    auto a = inner (membersArea);
    g.drawText ("Inputs", a.withHeight (18), juce::Justification::centredLeft);
    g.drawText ("Buses", a.withY (buses[0]->getY() - 20).withHeight (18), juce::Justification::centredLeft);

    g.setFont (rkfx::Fonts::regular (10.0f));
    const auto nameBelow = [&] (juce::Component& c, const juce::String& text)
    {
        if (c.isVisible())
            g.drawFittedText (text, c.getBounds().translated (0, kSwitchH + 1).withHeight (14).expanded (2, 0),
                              juce::Justification::centred, 1, 0.7f);
    };
    for (int ch = 0; ch < kChannels; ++ch)
        if (const auto n = owner.mixer.getChannelName (ch); n.isNotEmpty())
            nameBelow (*channels[(size_t) ch], n);
        else if (owner.isStereo (ch))
            nameBelow (*channels[(size_t) ch], owner.channelLabel (ch));
    for (int bus = 0; bus < kBuses; ++bus)
        if (const auto n = owner.mixer.getBusName (bus); n.isNotEmpty())
            nameBelow (*buses[(size_t) bus], n);
}

void RKMixEditor::GroupPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! muteGroup)
        if (const auto i = swatchAt (swatches, e.getPosition()); i >= 0)
            owner.mixer.setDcaColour (index, i);
}

//==============================================================================
juce::String RKMixEditor::dcaDisplayName (int dca) const
{
    const auto name = mixer.getDcaName (dca);
    return name.isNotEmpty() ? name : "DCA " + num (dca);
}

juce::String RKMixEditor::muteGroupDisplayName (int group) const
{
    const auto name = mixer.getMuteGroupName (group);
    return name.isNotEmpty() ? name : "Mute " + num (group);
}

juce::String RKMixEditor::stripDisplayName (int strip) const
{
    return strip >= RKMixProcessor::kUserPageBus ? busDisplayName (strip - RKMixProcessor::kUserPageBus) : displayName (strip);
}

void RKMixEditor::selectGroup (int i, bool muteGroup)
{
    selectedDca = muteGroup ? -1 : juce::jlimit (0, kDcas - 1, i);
    selectedMuteGroup = muteGroup ? juce::jlimit (0, kMuteGroups - 1, i) : -1;
    selectedBus = -1;
    selectedChain = -1;
    rebuildPanels();
    setPage (Page::strip);
}

void RKMixEditor::setSpill (int dca)
{
    spillDca = dca >= 0 && ! mixer.getDcaMembers (dca).isEmpty() ? dca : -1;
    if (spillDca >= 0)
        layer = kDcaLayer;
    for (auto& s : dcaStrips)
        s->refresh();
    if (auto* panel = dynamic_cast<GroupPanel*> (stripPanel.get()))
        panel->refresh();
    setLayer (layer);
}
