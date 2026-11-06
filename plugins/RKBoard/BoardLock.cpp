#include "BoardEditor.h"

// The window's lock for the show, as RKMix's (MixLock.cpp):
//
//   Footswitches and scenes: the scene buttons, the pedals' footswitches,
//   Tap, Tuner and Morph still work; knobs, dragging, adding and removing,
//   the routing, presets and right-click menus don't.
//   Everything: nothing changes; a pedal can still be selected to look at
//   its controls.
//
// Hold Lock for a second to unlock. Undo is off while locked. Host
// parameters, MIDI and the remote still work; the lock isn't saved.

namespace rkboard
{

using namespace rkfx;

namespace
{
    constexpr double kHoldMs = 1000.0;
}

void BoardEditor::LockShield::paint (juce::Graphics& g)
{
    // A faint hatch: the controls under it are visible but locked.
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), Style::kPanelRadius);
    g.setColour (Palette::warning.withAlpha (0.12f));
    for (int x = -getHeight(); x < getWidth(); x += 14)
        g.drawLine ((float) x, (float) getHeight(), (float) (x + getHeight()), 0.0f, 1.0f);
}

void BoardEditor::LockButton::paint (juce::Graphics& g)
{
    const auto locked = owner.isLocked();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (locked ? Palette::warning.withAlpha (0.85f) : Palette::panel.brighter (0.08f));
    g.fillRoundedRectangle (r, Style::kCornerRadius);

    // Held: how far to unlocking.
    if (holding)
    {
        const auto t = (float) juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - holdStart) / kHoldMs);
        g.setColour (Palette::text.withAlpha (0.35f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * t), Style::kCornerRadius);
    }

    g.setColour (locked ? juce::Colours::black : Palette::text);
    g.setFont (Fonts::bold (12.5f));
    const auto text = ! locked ? juce::String ("Lock") : holding ? juce::String ("Unlocking...")
                    : owner.board.getLock() == BoardProcessor::Lock::footswitches ? juce::String ("Locked: switches")
                                                                                  : juce::String ("Locked");
    g.drawFittedText (text, getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.8f);
}

void BoardEditor::LockButton::mouseDown (const juce::MouseEvent&)
{
    if (owner.isLocked())
    {
        holding = true;
        holdStart = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (30);
        repaint();
        return;
    }

    using Lock = BoardProcessor::Lock;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Lock for the show (hold Lock to unlock)");
    menu.addItem (1, "Footswitches and scenes only: scenes, footswitches, Tap, Tuner, Morph");
    menu.addItem (2, "Everything: nothing can be changed");
    juce::Component::SafePointer<BoardEditor> safe (&owner);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        safe->board.setLock (result == 1 ? Lock::footswitches : Lock::everything);
        safe->applyLock();
    });
}

void BoardEditor::LockButton::mouseUp (const juce::MouseEvent&)
{
    holding = false;
    stopTimer();
    repaint();
}

void BoardEditor::LockButton::timerCallback()
{
    if (holding && juce::Time::getMillisecondCounterHiRes() - holdStart >= kHoldMs)
    {
        holding = false;
        stopTimer();
        owner.board.setLock (BoardProcessor::Lock::off);
        owner.applyLock();
    }
    repaint();
}

//==============================================================================
void BoardEditor::applyLock()
{
    using Lock = BoardProcessor::Lock;
    const auto mode = board.getLock();
    shownLock = mode;
    const auto locked = mode != Lock::off;
    const auto everything = mode == Lock::everything;

    const auto place = [] (LockShield& shield, bool visible, juce::Rectangle<int> area)
    {
        shield.setVisible (visible && ! area.isEmpty());
        if (shield.isVisible())
        {
            shield.setBounds (area);
            shield.toFront (false);
        }
    };

    // The header: presets, Save, Assign, Undo (left), In and Out (right). Lock stays free.
    place (shields[0], locked, presetButton.getBounds().getUnion (redoButton.getBounds()).expanded (2, 2));
    place (shields[1], locked, inputLabel.getBounds().getUnion (outputSlider.getBounds()).expanded (2, 2));

    // The scene bar: all of it, or only the tempo field.
    const auto sceneBar = getLocalBounds().withTrimmedTop (kHeaderHeight).removeFromTop (kSceneBarHeight).reduced (kMargin - 2, 3);
    place (shields[2], locked, everything ? sceneBar : tempoField.getBounds().getUnion (tempoUnit.getBounds()).expanded (2, 2));

    // The pedal's (or the split's, the Assign list's, the tuner's) controls.
    place (shields[3], locked, panelArea());

    strip.setLock (mode);
    for (auto& button : sceneButtons)
        button.setLocked (locked);
    tempoField.setEditable (board.isStandalone() && ! locked);

    refreshUndo();
    lockButton.setTooltip (locked ? "Locked: hold for a second to unlock" : "Lock the window for the show");
    lockButton.repaint();
}

} // namespace rkboard
