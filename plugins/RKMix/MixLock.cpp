#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MixEditorStyle.h"

using namespace MixGui::Layout;
using namespace mix;

// Undo and redo, and the show lock: protecting the console from a stray
// click during the show.
//
//   Faders and mutes: the faders (channels, buses, DCAs, main), mutes, solo,
//   mute groups, Spill, scenes, Talk and Clear solo still work; the top
//   panel, pans, inserts, names, right-click menus and the solo setup don't.
//   Everything: nothing changes; the pages can still be looked at.
//   Either way, no preset is loaded and Bypass can't be clicked.
//
// Hold Lock for a second to unlock. Undo is off while locked.

namespace
{
    constexpr double kHoldMs = 1000.0;
}

//==============================================================================
void RKMixEditor::LockShield::paint (juce::Graphics& g)
{
    // A faint hatch: the controls under it are visible but locked.
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), rkfx::Style::kPanelRadius);
    g.setColour (rkfx::Palette::warning.withAlpha (0.12f));
    for (int x = -getHeight(); x < getWidth(); x += 14)
        g.drawLine ((float) x, (float) getHeight(), (float) (x + getHeight()), 0.0f, 1.0f);
}

//==============================================================================
void RKMixEditor::LockButton::paint (juce::Graphics& g)
{
    const auto locked = owner.isLocked();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (locked ? rkfx::Palette::warning.withAlpha (0.85f) : rkfx::Palette::panel.brighter (0.08f));
    g.fillRoundedRectangle (r, 4.0f);

    // Held: how far to unlocking.
    if (holding)
    {
        const auto t = (float) juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - holdStart) / kHoldMs);
        g.setColour (rkfx::Palette::text.withAlpha (0.35f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * t), 4.0f);
    }

    g.setColour (locked ? juce::Colours::black : rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (12.5f));
    const auto text = ! locked ? juce::String ("Lock") : holding ? juce::String ("Unlocking...")
                    : owner.mixer.getLock() == RKMixProcessor::Lock::faders ? juce::String ("Locked: faders") : juce::String ("Locked");
    g.drawText (text, getLocalBounds(), juce::Justification::centred);
}

void RKMixEditor::LockButton::mouseDown (const juce::MouseEvent&)
{
    if (owner.isLocked())
    {
        holding = true;
        holdStart = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (30);
        repaint();
        return;
    }

    using Lock = RKMixProcessor::Lock;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Show lock (hold Lock to unlock)");
    menu.addItem (1, "Faders and mutes only: faders, mutes, solo, mute groups, scenes, Talk");
    menu.addItem (2, "Everything: nothing can be changed");
    juce::Component::SafePointer<RKMixEditor> safe (&owner);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        safe->mixer.setLock (result == 1 ? Lock::faders : Lock::everything);
        safe->applyLock();
    });
}

void RKMixEditor::LockButton::mouseUp (const juce::MouseEvent&)
{
    holding = false;
    stopTimer();
    repaint();
}

void RKMixEditor::LockButton::timerCallback()
{
    if (holding && juce::Time::getMillisecondCounterHiRes() - holdStart >= kHoldMs)
    {
        holding = false;
        stopTimer();
        owner.mixer.setLock (RKMixProcessor::Lock::off);
        owner.applyLock();
    }
    repaint();
}

//==============================================================================
bool RKMixEditor::isLocked() const noexcept
{
    return mixer.getLock() != RKMixProcessor::Lock::off;
}

void RKMixEditor::applyLock()
{
    using Lock = RKMixProcessor::Lock;
    const auto mode = mixer.getLock();
    const auto locked = mode != Lock::off;
    const auto everything = mode == Lock::everything;
    if (locked)
        closeInsert();

    const auto place = [this] (LockShield& shield, bool visible, juce::Rectangle<int> area)
    {
        shield.setVisible (visible && ! area.isEmpty());
        if (shield.isVisible())
        {
            shield.setBounds (area);
            shield.toFront (false);
        }
    };
    place (shields[0], locked, panelArea().expanded (2));

    // The master section: with faders and mutes, Clear solo and Talk stay free (and Back to faders).
    const auto master = masterArea().removeFromTop (kTopHeight);
    if (everything)
    {
        place (shields[1], true, master);
        place (shields[2], false, {});
    }
    else
    {
        place (shields[1], locked, master.withBottom (clearSolo.getY() - 2));
        const auto bottom = sendsBack.isVisible() ? sendsBack.getY() - 2 : master.getBottom();
        place (shields[2], locked, master.withTop (oscButton->getY() - 2).withBottom (bottom));
    }
    place (shields[3], everything, contentArea().withTrimmedTop (kTopHeight + kGap));
    place (shields[4], everything, sceneButtons.front().getBounds().getUnion (sceneButtons.back().getBounds()).expanded (2));

    // Never a preset loaded or the console bypassed (silence) by a stray click.
    place (shields[5], locked, juce::Rectangle<int> (kWidth / 2, 0, kWidth / 2, rkfx::EditorBase::kHeaderHeight));
    place (shields[6], locked, globalArea().removeFromRight (110).withSizeKeepingCentre (110, 40));

    // With faders and mutes: no pans, names or menus on the strips.
    for (auto* list : { &strips, &busStrips })
        for (auto& s : *list)
        {
            s->pan.setInterceptsMouseClicks (! locked, false);
            s->name.setEditable (false, ! locked, false);
        }
    for (auto* list : { &dcaStrips, &muteGroupStrips })
        for (auto& s : *list)
            s->name.setEditable (false, ! locked, false);

    refreshUndo();
    lockButton.setTooltip (locked ? "Locked: hold for a second to unlock" : "Lock the console for the show");
    lockButton.repaint();
}

void RKMixEditor::refreshUndo()
{
    auto& undo = mixer.getUndo();
    lastUndoVersion = undo.getVersion();
    const auto locked = isLocked();
    undoButton.setEnabled (! locked && undo.canUndo());
    redoButton.setEnabled (! locked && undo.canRedo());
    undoButton.setTooltip (undo.canUndo() ? "Undo " + undo.undoDescription() + " (Cmd+Z)" : juce::String ("Nothing to undo"));
    redoButton.setTooltip (undo.canRedo() ? "Redo " + undo.redoDescription() + " (Cmd+Shift+Z)" : juce::String ("Nothing to redo"));
}

bool RKMixEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        if (! isLocked())
        {
            if (key.getModifiers().isShiftDown())
                mixer.getUndo().redo();
            else
                mixer.getUndo().undo();
            refreshUndo();
        }
        return true;
    }
    return rkfx::EditorBase::keyPressed (key);
}
