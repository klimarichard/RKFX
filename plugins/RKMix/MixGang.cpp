#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"

using namespace MixGui::Layout;
using namespace mix;

// Ganging and copy / paste.
//
// Cmd- or Shift-click strips' names to gang them (click again to take one
// out, Esc or the strip menu's Ungang to end it). A ganged fader moved by hand
// moves the others by the same travel, each from where it was when the move
// began (so offsets survive one hitting the bottom); a ganged Mute sets the
// others' too. Right-click a section of the channel page (or the EQ,
// compressor, limiter of a bus or the main) to copy it, or paste what was
// copied: onto the ganged channels too when this one is ganged.

//==============================================================================
int RKMixEditor::Strip::id() const noexcept
{
    return isBus() ? RKMixProcessor::kUserPageBus + bus : channel;
}

RKMixEditor::Strip* RKMixEditor::stripFor (int stripId) const
{
    if (stripId >= RKMixProcessor::kUserPageBus)
        return juce::isPositiveAndBelow (stripId - RKMixProcessor::kUserPageBus, (int) busStrips.size())
                   ? busStrips[(size_t) (stripId - RKMixProcessor::kUserPageBus)].get() : nullptr;
    return juce::isPositiveAndBelow (stripId, (int) strips.size()) ? strips[(size_t) stripId].get() : nullptr;
}

void RKMixEditor::toggleGang (int stripId)
{
    // The first Cmd-click gangs the selected strip with the one clicked.
    if (gang.isEmpty())
    {
        if (selectedBus >= 0)
            gang.add (RKMixProcessor::kUserPageBus + selectedBus);
        else if (isChannelSelected())
            gang.add (selected);
    }
    if (gang.contains (stripId))
        gang.removeFirstMatchingValue (stripId);
    else
        gang.add (stripId);
    if (gang.size() < 2)
        gang.clearQuick();
    for (auto* list : { &strips, &busStrips })
        for (auto& s : *list)
            s->repaint();
}

void RKMixEditor::clearGang()
{
    if (gang.isEmpty())
        return;
    gang.clearQuick();
    for (auto* list : { &strips, &busStrips })
        for (auto& s : *list)
            s->repaint();
}

void RKMixEditor::gangGestureStart (Strip& leader)
{
    gangStarts.clear();
    gangLeader = isGanged (leader.id()) ? &leader : nullptr;
    if (gangLeader == nullptr)
        return;
    for (const auto other : gang)
        if (auto* s = stripFor (other); s != nullptr && s != &leader && s->fader != nullptr)
        {
            auto& p = s->fader->getParameter();
            gangStarts.emplace_back (&p, p.getValue());
        }
}

void RKMixEditor::gangMove (Strip& leader, float travel)
{
    if (gangLeader != &leader)
        return;
    for (auto& [param, start] : gangStarts)
    {
        const auto value = juce::jlimit (0.0f, 1.0f, start + travel);
        if (! juce::exactlyEqual (param->getValue(), value))
            param->setValueNotifyingHost (value);
    }
}

void RKMixEditor::gangMute (Strip& leader)
{
    if (! isGanged (leader.id()))
        return;
    const auto on = mixer.findParam (leader.isBus() ? busMuteId (leader.bus) : channelMuteId (leader.channel))->getValue();
    for (const auto other : gang)
        if (auto* s = stripFor (other); s != nullptr && s != &leader)
        {
            auto* p = mixer.findParam (s->isBus() ? busMuteId (s->bus) : channelMuteId (s->channel));
            if (! juce::exactlyEqual (p->getValue(), on))
                p->setValueNotifyingHost (on);
        }
}

//==============================================================================
juce::Array<int> RKMixEditor::pasteTargets (int ch) const
{
    juce::Array<int> targets;
    if (isGanged (ch))
    {
        for (const auto s : gang)
            if (s < RKMixProcessor::kUserPageBus)
                targets.add (s);
    }
    else
        targets.add (ch);
    return targets;
}

void RKMixEditor::addCopyPasteItems (juce::PopupMenu& menu, int channel, int chain, int section, int firstId) const
{
    using Section = RKMixProcessor::Section;
    if (section >= 0)
        menu.addItem (firstId, "Copy " + RKMixProcessor::sectionName ((Section) section));
    menu.addItem (firstId + 1, "Copy whole strip (with inserts)");

    const auto fits = channel >= 0 ? RKMixProcessor::isChannelCopy (clipboard) : RKMixProcessor::isChainCopy (clipboard);
    auto text = juce::String ("Paste");
    if (fits)
    {
        text << " " << RKMixProcessor::describeCopy (clipboard);
        if (channel >= 0)
            if (const auto n = pasteTargets (channel).size(); n > 1)
                text << " to " << n << " ganged channels";
    }
    menu.addItem (firstId + 2, text, fits);
}

bool RKMixEditor::handleCopyPaste (int result, int channel, int chain, int section, int firstId)
{
    using Section = RKMixProcessor::Section;
    if (result == firstId || result == firstId + 1)
    {
        const auto s = result == firstId ? (Section) section : Section::whole;
        clipboard = channel >= 0 ? mixer.copyChannel (channel, s) : mixer.copyChain (chain, s);
        return true;
    }
    if (result == firstId + 2)
    {
        if (channel >= 0)
            for (const auto ch : pasteTargets (channel))
                mixer.pasteChannel (ch, clipboard);
        else
            mixer.pasteChain (chain, clipboard);
        return true;
    }
    return false;
}

void RKMixEditor::showSectionMenu (int channel, int chain, int section, juce::Component& target, juce::Point<int> where)
{
    if (isLocked())
        return;
    constexpr int kFirst = 1;
    juce::PopupMenu menu;
    addCopyPasteItems (menu, channel, chain, section, kFirst);
    juce::Component::SafePointer<RKMixEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (target.localAreaToGlobal (juce::Rectangle<int> (where, where).expanded (1))),
                        [safe, channel, chain, section] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->handleCopyPaste (result, channel, chain, section, kFirst);
    });
}
