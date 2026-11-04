#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kMapHeight = 104;
    constexpr int kPadsWidth = 340;
    constexpr int kGap = 10;

    constexpr int whitesBefore[12] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
    bool isBlack (int note) { return (0x54a >> (note % 12)) & 1; }
    int whiteIndex (int note) { return note / 12 * 7 + whitesBefore[note % 12]; }
    constexpr int kWhiteKeys = 75;   // 0..127

    juce::String zoneText (int low, int high)
    {
        if (low > high)
            std::swap (low, high);
        return low == high ? CueParams::noteName (low) : CueParams::noteName (low) + juce::String::fromUTF8 ("–") + CueParams::noteName (high);
    }

    void setParam (juce::RangedAudioParameter& p, float value)
    {
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.convertTo0to1 (value));
        p.endChangeGesture();
    }
}

//==============================================================================
RKCueEditor::Footswitch::Footswitch (juce::RangedAudioParameter& p, const juce::String& text)
    : juce::TextButton (text), param (p)
{
    rkfx::tagParameter (*this, p.getParameterID());
    onStateChange = [this]
    {
        const auto pressed = isDown();
        if (pressed == (param.getValue() >= 0.5f))
            return;
        param.beginChangeGesture();
        param.setValueNotifyingHost (pressed ? 1.0f : 0.0f);
        param.endChangeGesture();
    };
}

//==============================================================================
juce::Rectangle<float> RKCueEditor::KeyMap::keysArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop ((float) (kLanes * kLaneHeight + 4));
}

float RKCueEditor::KeyMap::keyX (int key) const
{
    const auto keys = keysArea();
    const auto w = keys.getWidth() / (float) kWhiteKeys;
    const auto wi = (float) whiteIndex (key);
    return keys.getX() + (isBlack (key) ? wi * w : (wi + 0.5f) * w);
}

int RKCueEditor::KeyMap::noteAt (float x) const
{
    auto best = 0;
    for (int n = 1; n < 128; ++n)
        if (std::abs (keyX (n) - x) < std::abs (keyX (best) - x))
            best = n;
    return best;
}

std::array<int, 16> RKCueEditor::KeyMap::lanes() const
{
    // Overlapping zones go on separate lanes (the selected slot's too).
    std::array<int, 16> lane {};
    std::array<int, kLanes> laneEnd;
    laneEnd.fill (-1);
    std::vector<std::pair<int, int>> order;   // (low, slot)
    for (int s = 0; s < 16; ++s)
    {
        const auto lo = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "low")));
        const auto hi = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "high")));
        order.push_back ({ juce::jmin (lo, hi), s });
    }
    std::sort (order.begin(), order.end());
    for (const auto& [lo, s] : order)
    {
        if (! editor.cue.getSlot (s).hasFile() && s != editor.selected)
            continue;
        const auto hi = juce::jmax (lo, juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "high"))),
                                    juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "low"))));
        auto l = 0;
        while (l < kLanes - 1 && laneEnd[(size_t) l] >= lo)
            ++l;
        lane[(size_t) s] = l;
        laneEnd[(size_t) l] = juce::jmax (laneEnd[(size_t) l], hi);
    }
    return lane;
}

juce::Rectangle<float> RKCueEditor::KeyMap::zoneBar (int slot) const
{
    auto lo = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (slot, "low")));
    auto hi = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (slot, "high")));
    if (lo > hi)
        std::swap (lo, hi);
    const auto half = keysArea().getWidth() / (float) kWhiteKeys * 0.5f;
    const auto lane = lanes()[(size_t) slot];
    // A pixel short at each end, so zones on neighbouring keys stay apart.
    return { keyX (lo) - half + 1.0f, (float) (lane * kLaneHeight), keyX (hi) - keyX (lo) + 2.0f * half - 2.0f, (float) kLaneHeight - 2.0f };
}

void RKCueEditor::KeyMap::paint (juce::Graphics& g)
{
    const auto keys = keysArea();
    const auto w = keys.getWidth() / (float) kWhiteKeys;
    const auto accent = editor.accent();
    const auto& engine = editor.cue.getEngine();

    // White keys, then black ones on top.
    for (int n = 0; n < 128; ++n)
        if (! isBlack (n))
        {
            const juce::Rectangle<float> r (keys.getX() + (float) whiteIndex (n) * w, keys.getY(), w, keys.getHeight());
            g.setColour (engine.isKeyDown (n) ? accent : juce::Colour (0xffd8d8dc));
            g.fillRect (r.reduced (0.5f, 0.0f));
            if (n % 12 == 0 && n >= 24)
            {
                g.setColour (juce::Colour (0xff55585e));
                g.setFont (rkfx::Fonts::regular (8.5f));
                g.drawText ("C" + juce::String (n / 12 - 2), r.withTrimmedTop (r.getHeight() - 12.0f), juce::Justification::centred);
            }
        }
    for (int n = 0; n < 128; ++n)
        if (isBlack (n))
        {
            const juce::Rectangle<float> r (keyX (n) - w * 0.32f, keys.getY(), w * 0.64f, keys.getHeight() * 0.6f);
            g.setColour (engine.isKeyDown (n) ? accent.darker (0.3f) : juce::Colour (0xff202124));
            g.fillRect (r);
        }

    // The zones: the selected one in full accent, playing ones brighter.
    for (int pass = 0; pass < 2; ++pass)
        for (int s = 0; s < 16; ++s)
        {
            const auto isSelected = s == editor.selected;
            if ((pass == 1) != isSelected || (! editor.cue.getSlot (s).hasFile() && ! isSelected))
                continue;
            const auto bar = zoneBar (s);
            const auto playing = engine.getCopies (s) > 0;
            g.setColour (isSelected ? accent : accent.withAlpha (playing ? 0.85f : 0.4f));
            g.fillRoundedRectangle (bar, 2.5f);
            if (! editor.cue.getSlot (s).hasFile())
            {
                g.setColour (rkfx::Palette::background.withAlpha (0.5f));
                g.fillRoundedRectangle (bar.reduced (1.5f), 2.0f);
            }

            const auto rootNote = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "root")));
            const auto pitched = rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "pitch")) >= 0.5f;
            if (pitched)
            {
                g.setColour (rkfx::Palette::onAccent (accent));
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ keyX (rootNote), bar.getCentreY() }));
            }
        }
}

void RKCueEditor::KeyMap::mouseDown (const juce::MouseEvent& e)
{
    grab = Grab::none;
    grabParam = nullptr;
    const auto p = e.position;

    // The selected zone first, then the others.
    std::vector<int> order { editor.selected };
    for (int s = 0; s < 16; ++s)
        if (s != editor.selected && editor.cue.getSlot (s).hasFile())
            order.push_back (s);

    for (const auto s : order)
    {
        const auto bar = zoneBar (s);
        if (! bar.expanded (4.0f, 2.0f).contains (p))
            continue;

        grabSlot = s;
        const auto rootX = keyX (juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "root"))));
        const auto pitched = rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "pitch")) >= 0.5f;
        const auto lowIsLeft = rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "low")) <= rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "high"));
        if (pitched && std::abs (p.x - rootX) < 5.0f)
            grab = Grab::root;
        else if (std::abs (p.x - bar.getX()) < 6.0f)
            grab = lowIsLeft ? Grab::low : Grab::high;
        else if (std::abs (p.x - bar.getRight()) < 6.0f)
            grab = lowIsLeft ? Grab::high : Grab::low;

        if (grab != Grab::none)
        {
            grabParam = &editor.slotParam (s, grab == Grab::root ? "root" : grab == Grab::low ? "low" : "high");
            grabParam->beginChangeGesture();
        }
        editor.select (s);
        return;
    }

    // A key: select the first zone holding it.
    if (keysArea().contains (p))
    {
        const auto n = noteAt (p.x);
        for (int s = 0; s < 16; ++s)
            if (editor.cue.getSlot (s).hasFile())
            {
                const auto lo = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "low")));
                const auto hi = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "high")));
                if (n >= juce::jmin (lo, hi) && n <= juce::jmax (lo, hi))
                {
                    editor.select (s);
                    return;
                }
            }
    }
}

void RKCueEditor::KeyMap::mouseDrag (const juce::MouseEvent& e)
{
    if (grabParam == nullptr)
        return;
    const auto n = noteAt (e.position.x);
    grabParam->setValueNotifyingHost (grabParam->convertTo0to1 ((float) n));
    repaint();
}

void RKCueEditor::KeyMap::mouseUp (const juce::MouseEvent&)
{
    if (grabParam != nullptr)
        grabParam->endChangeGesture();
    grabParam = nullptr;
    grab = Grab::none;
}

//==============================================================================
juce::Rectangle<int> RKCueEditor::Pads::padBounds (int slot) const
{
    const auto w = getWidth() / 4, h = getHeight() / 4;
    const auto row = slot / 4, col = slot % 4;   // slot 1 bottom left, as on a pad controller
    return juce::Rectangle<int> (col * w, (3 - row) * h, w, h).reduced (3);
}

juce::Rectangle<int> RKCueEditor::Pads::playBounds (int slot) const
{
    return padBounds (slot).removeFromTop (24).removeFromRight (26);
}

int RKCueEditor::Pads::padAt (juce::Point<int> p) const
{
    for (int s = 0; s < 16; ++s)
        if (padBounds (s).contains (p))
            return s;
    return -1;
}

void RKCueEditor::Pads::paint (juce::Graphics& g)
{
    const auto accent = editor.accent();
    const auto& engine = editor.cue.getEngine();

    for (int s = 0; s < 16; ++s)
    {
        const auto r = padBounds (s).toFloat();
        const auto& slot = editor.cue.getSlot (s);
        const auto copies = engine.getCopies (s);

        g.setColour (copies > 0 ? accent.withAlpha (0.45f) : rkfx::Palette::knobBody);
        g.fillRoundedRectangle (r, 5.0f);
        if (s == editor.selected || s == dropSlot)
        {
            g.setColour (s == dropSlot ? rkfx::Palette::text : accent);
            g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 2.0f);
        }

        auto inner = r.reduced (7.0f, 5.0f);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (11.0f));
        g.drawText (juce::String (s + 1), inner.removeFromTop (14.0f), juce::Justification::centredLeft);

        const auto lo = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "low")));
        const auto hi = juce::roundToInt (rkfx::ProcessorBase::valueOf (&editor.slotParam (s, "high")));
        g.setFont (rkfx::Fonts::regular (10.5f));
        g.drawText (zoneText (lo, hi), inner.removeFromBottom (13.0f), juce::Justification::centredLeft);

        if (slot.hasFile())
        {
            g.setColour (rkfx::Palette::text);
            g.setFont (rkfx::Fonts::bold (12.5f));
            g.drawFittedText (slot.name, inner.toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);

            // Play.
            const auto play = playBounds (s).toFloat().reduced (7.0f, 6.0f);
            juce::Path triangle;
            triangle.addTriangle (play.getX(), play.getY(), play.getX(), play.getBottom(), play.getRight(), play.getCentreY());
            g.setColour (playing == s ? accent : rkfx::Palette::text.withAlpha (0.8f));
            g.fillPath (triangle);

            // Progress of the newest copy.
            if (copies > 0)
            {
                auto line = r.reduced (6.0f, 0.0f).removeFromBottom (3.0f).translated (0.0f, -2.0f);
                g.setColour (accent);
                g.fillRect (line.withWidth (line.getWidth() * engine.getProgress (s)));
            }
        }
        else
        {
            g.setColour (rkfx::Palette::textDim.withAlpha (0.7f));
            g.setFont (rkfx::Fonts::regular (11.0f));
            g.drawFittedText ("drop a MIDI file", inner.toNearestInt(), juce::Justification::centredLeft, 2);
        }
    }
}

void RKCueEditor::Pads::mouseDown (const juce::MouseEvent& e)
{
    const auto s = padAt (e.getPosition());
    if (s < 0)
        return;
    if (playBounds (s).contains (e.getPosition()) && editor.cue.getSlot (s).hasFile() && ! e.mods.isPopupMenu())
    {
        playing = s;
        auto& p = *editor.cue.findParam (CueParams::playId (s));
        p.beginChangeGesture();
        p.setValueNotifyingHost (1.0f);
    }
    editor.select (s);
}

void RKCueEditor::Pads::mouseUp (const juce::MouseEvent&)
{
    if (playing < 0)
        return;
    auto& p = *editor.cue.findParam (CueParams::playId (playing));
    p.setValueNotifyingHost (0.0f);
    p.endChangeGesture();
    playing = -1;
    repaint();
}

bool RKCueEditor::Pads::isInterestedInFileDrag (const juce::StringArray& paths)
{
    for (const auto& path : paths)
        if (juce::File (path).hasFileExtension ("mid;midi;smf"))
            return true;
    return false;
}

void RKCueEditor::Pads::filesDropped (const juce::StringArray& paths, int x, int y)
{
    dropSlot = -1;
    auto s = padAt ({ x, y });
    if (s < 0)
        return;

    // Several files fill this pad and the ones after it.
    for (const auto& path : paths)
    {
        const juce::File f (path);
        if (! f.hasFileExtension ("mid;midi;smf"))
            continue;
        if (s >= 16)
            break;
        editor.loadInto (s++, f);
    }
    editor.select (padAt ({ x, y }));
}

//==============================================================================
RKCueEditor::RKCueEditor (RKCueProcessor& p)
    : rkfx::EditorBase (p, 900, 640), cue (p)
{
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    addToCanvas (keyMap);
    addToCanvas (pads);

    nameLabel.setEditable (false, true, false);
    nameLabel.setFont (rkfx::Fonts::bold (15.0f));
    nameLabel.setColour (juce::Label::textColourId, rkfx::Palette::text);
    nameLabel.setTooltip ("Double-click to rename");
    nameLabel.onTextChange = [this] { cue.setSlotName (selected, nameLabel.getText().trim()); };
    addToCanvas (nameLabel);

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a MIDI file into slot " + juce::String (selected + 1), juce::File(), "*.mid;*.midi;*.smf");
        juce::Component::SafePointer<RKCueEditor> safe (this);
        const auto slot = selected;
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe, slot] (const juce::FileChooser& fc)
                              {
                                  if (safe != nullptr && fc.getResult() != juce::File())
                                      safe->loadInto (slot, fc.getResult());
                              });
    };
    clearButton.onClick = [this] { cue.clearSlot (selected); note.clear(); };
    learnButton.setTooltip ("Press a key to set the zone (Shift-click: press two keys for a range)");
    learnButton.onClick = [this]
    {
        if (learn != Learn::off)
            learn = Learn::off;
        else
        {
            learn = juce::ModifierKeys::getCurrentModifiers().isShiftDown() ? Learn::rangeFirst : Learn::one;
            learnNoteCount = cue.getEngine().getNoteCount();
        }
        updateLearnButton();
    };
    learnButton.setClickingTogglesState (false);
    addToCanvas (loadButton);
    addToCanvas (clearButton);
    addToCanvas (learnButton);

    tempoSource = std::make_unique<rkfx::SegmentedSwitch> (param ("tempo_source"), accent(), juce::StringArray {}, "Tempo Source");
    tempo       = std::make_unique<rkfx::Knob> (param ("tempo"), "Tempo");
    trigChannel = std::make_unique<rkfx::Knob> (param ("trig_channel"), "Trigger Ch");
    thru        = std::make_unique<rkfx::ToggleSwitch> (param ("thru"), accent(), "Thru");
    stopAll     = std::make_unique<Footswitch> (param ("stop_all"), "Stop All");
    for (juce::Component* c : std::initializer_list<juce::Component*> { tempoSource.get(), tempo.get(), trigChannel.get(), thru.get(), stopAll.get() })
        addToCanvas (*c);

    tempoWatcher = std::make_unique<juce::ParameterAttachment> (param ("tempo_source"), [this] (float v) { tempo->setDimmed (juce::roundToInt (v) != 2); });
    tempoWatcher->sendInitialUpdate();

    buildSlotControls();
    finishSetup();
    refresh();
    refresher.startTimerHz (20);
}

juce::RangedAudioParameter& RKCueEditor::slotParam (int slot, const char* name) const
{
    return *cue.findParam (CueParams::slotId (slot, name));
}

void RKCueEditor::select (int slot)
{
    if (slot < 0 || slot >= 16 || slot == selected)
        return;
    selected = slot;
    if (learn != Learn::off)
    {
        learn = Learn::off;
        updateLearnButton();
    }
    note.clear();
    buildSlotControls();
    repaint();
}

void RKCueEditor::buildSlotControls()
{
    using rkfx::Knob;
    const auto s = selected;

    low       = std::make_unique<Knob> (slotParam (s, "low"), "Low Key");
    high      = std::make_unique<Knob> (slotParam (s, "high"), "High Key");
    root      = std::make_unique<Knob> (slotParam (s, "root"), "Root Key");
    transpose = std::make_unique<Knob> (slotParam (s, "transpose"), "Transpose");
    velocity  = std::make_unique<Knob> (slotParam (s, "velocity"), "Velocity");
    channel   = std::make_unique<Knob> (slotParam (s, "channel"), "Channel");
    mode      = std::make_unique<rkfx::SegmentedSwitch> (slotParam (s, "mode"), accent());
    trackPitch = std::make_unique<rkfx::ToggleSwitch> (slotParam (s, "pitch"), accent(), "Track Pitch");
    keepDrums  = std::make_unique<rkfx::ToggleSwitch> (slotParam (s, "drums"), accent(), "Keep Drums");
    loop       = std::make_unique<rkfx::ToggleSwitch> (slotParam (s, "loop"), accent(), "Loop");
    poly       = std::make_unique<rkfx::ToggleSwitch> (slotParam (s, "poly"), accent(), "Poly");
    stopSlot   = std::make_unique<Footswitch> (*cue.findParam (CueParams::stopId (s)), "Stop");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             low.get(), high.get(), root.get(), transpose.get(), velocity.get(), channel.get(), mode.get(),
             trackPitch.get(), keepDrums.get(), loop.get(), poly.get(), stopSlot.get() })
        addToCanvas (*c);

    pitchWatcher = std::make_unique<juce::ParameterAttachment> (slotParam (s, "pitch"), [this] (float v) { root->setDimmed (v < 0.5f); keyMap.repaint(); });
    pitchWatcher->sendInitialUpdate();

    showSlotName();
    layoutContent();
}

void RKCueEditor::showSlotName()
{
    const auto& slot = cue.getSlot (selected);
    nameLabel.setText (slot.hasFile() ? slot.name : juce::String(), juce::dontSendNotification);
    nameLabel.setEditable (false, slot.hasFile(), false);
    clearButton.setEnabled (slot.hasFile());
}

void RKCueEditor::loadInto (int slot, const juce::File& file)
{
    const auto error = cue.loadFile (slot, file);
    note = error.isNotEmpty() ? file.getFileName() + ": " + error : juce::String();
    if (error.isEmpty() && learn == Learn::off)
        select (slot);
    repaint();
}

void RKCueEditor::setNote (int slot, const char* name, int noteNumber)
{
    setParam (slotParam (slot, name), (float) noteNumber);
}

void RKCueEditor::updateLearnButton()
{
    learnButton.setToggleState (learn != Learn::off, juce::dontSendNotification);
    learnButton.setButtonText (learn == Learn::off ? "Learn" : learn == Learn::rangeSecond ? "2nd key..." : "Press a key");
}

//==============================================================================
juce::Rectangle<int> RKCueEditor::mapArea() const  { return contentArea().withHeight (kMapHeight); }
juce::Rectangle<int> RKCueEditor::padsArea() const { return contentArea().withTrimmedTop (kMapHeight + kGap).withWidth (kPadsWidth); }
juce::Rectangle<int> RKCueEditor::slotArea() const { return contentArea().withTrimmedTop (kMapHeight + kGap).withTrimmedLeft (kPadsWidth + kGap); }

void RKCueEditor::layoutContent()
{
    using rkfx::Knob;

    keyMap.setBounds (mapArea().reduced (8, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (8));
    pads.setBounds (padsArea().reduced (5, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (5));

    auto area = slotArea().reduced (8, 0).withTrimmedTop (kTitleHeight);
    auto top = area.removeFromTop (28);
    stopSlot->setBounds (top.removeFromRight (64));
    top.removeFromRight (6);
    learnButton.setBounds (top.removeFromRight (96));
    top.removeFromRight (6);
    clearButton.setBounds (top.removeFromRight (60));
    top.removeFromRight (6);
    loadButton.setBounds (top.removeFromRight (60));
    top.removeFromRight (6);
    nameLabel.setBounds (top);
    area.removeFromTop (22);   // the file's info, painted

    auto knobs = area.removeFromTop (Knob::kDefaultHeight);
    const auto sixth = knobs.getWidth() / 6;
    for (auto* k : { low.get(), high.get(), root.get(), transpose.get(), velocity.get(), channel.get() })
        k->setBounds (knobs.removeFromLeft (sixth));
    area.removeFromTop (6);

    auto row = area.removeFromTop (28);
    mode->setBounds (row.removeFromLeft (230));
    row.removeFromLeft (10);
    const auto toggle = (row.getWidth() - 6) / 2;
    loop->setBounds (row.removeFromLeft (toggle));
    row.removeFromLeft (6);
    poly->setBounds (row);
    area.removeFromTop (6);

    row = area.removeFromTop (28);
    trackPitch->setBounds (row.removeFromLeft (112));
    row.removeFromLeft (6);
    keepDrums->setBounds (row.removeFromLeft (112));

    // The bottom row.
    auto global = globalArea();
    tempoSource->setBounds (global.removeFromLeft (200).withSizeKeepingCentre (200, 48));
    global.removeFromLeft (kGap);
    tempo->setBounds (global.removeFromLeft (Knob::kDefaultWidth).withSizeKeepingCentre (Knob::kDefaultWidth, Knob::kDefaultHeight));
    global.removeFromLeft (kGap);
    trigChannel->setBounds (global.removeFromLeft (Knob::kDefaultWidth).withSizeKeepingCentre (Knob::kDefaultWidth, Knob::kDefaultHeight));
    global.removeFromLeft (kGap * 2);
    thru->setBounds (global.removeFromLeft (70).withSizeKeepingCentre (70, 28));
    global.removeFromLeft (kGap * 2);
    stopAll->setBounds (global.removeFromLeft (100).withSizeKeepingCentre (100, 40));
}

void RKCueEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, mapArea(), "Zones");
    drawSection (g, padsArea(), "Pads");
    drawSection (g, slotArea(), "Slot " + juce::String (selected + 1));

    const auto& slot = cue.getSlot (selected);
    auto area = slotArea().reduced (8, 0).withTrimmedTop (kTitleHeight + 28);
    auto info = area.removeFromTop (22);
    g.setFont (rkfx::Fonts::regular (12.0f));
    if (note.isNotEmpty())
    {
        g.setColour (rkfx::Palette::warning);
        g.drawText (note, info, juce::Justification::centredLeft, true);
    }
    else if (slot.hasFile())
    {
        g.setColour (rkfx::Palette::textDim);
        g.drawText (juce::String (slot.lengthBeats, 1) + " beats, " + juce::String (slot.lengthSeconds, 1) + " s at the file's tempo ("
                        + juce::String (slot.tempoBpm, 1) + " BPM), " + juce::String ((int) slot.tracks.size())
                        + ((int) slot.tracks.size() == 1 ? " track" : " tracks"),
                    info, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (rkfx::Palette::textDim);
        g.drawText ("Empty: Load a MIDI file, or drop one on a pad", info, juce::Justification::centredLeft);
    }

    // The file's tracks, in two columns.
    area.removeFromTop (rkfx::Knob::kDefaultHeight + 6 + 28 + 6 + 28 + 10);
    if (slot.hasFile())
    {
        const auto columnWidth = area.getWidth() / 2;
        const auto rows = juce::jmax (1, area.getHeight() / 17);
        g.setFont (rkfx::Fonts::regular (12.0f));
        for (int t = 0; t < (int) slot.tracks.size() && t < rows * 2; ++t)
        {
            auto r = juce::Rectangle<int> (area.getX() + (t / rows) * columnWidth, area.getY() + (t % rows) * 17, columnWidth - 12, 17);
            const auto& track = slot.tracks[(size_t) t];
            g.setColour (rkfx::Palette::text);
            g.drawText (track.name, r, juce::Justification::centredLeft, true);
            g.setColour (rkfx::Palette::textDim);
            g.drawText (juce::String (track.notes) + " notes", r, juce::Justification::centredRight);
        }
    }
}

void RKCueEditor::refresh()
{
    const auto& engine = cue.getEngine();

    // Learn: the newest key.
    if (learn != Learn::off && engine.getNoteCount() != learnNoteCount)
    {
        learnNoteCount = engine.getNoteCount();
        const auto n = engine.getLastNote();
        if (learn == Learn::rangeSecond)
        {
            setNote (selected, "low", juce::jmin (learnFirst, n));
            setNote (selected, "high", juce::jmax (learnFirst, n));
            setNote (selected, "root", learnFirst);
            learn = Learn::off;
        }
        else
        {
            setNote (selected, "low", n);
            setNote (selected, "high", n);
            setNote (selected, "root", n);
            learnFirst = n;
            learn = learn == Learn::rangeFirst ? Learn::rangeSecond : Learn::off;
        }
        updateLearnButton();
    }

    if (cue.getFilesVersion() != shownVersion)
    {
        shownVersion = cue.getFilesVersion();
        showSlotName();
        pads.repaint();
        keyMap.repaint();
        repaint (slotArea());
    }

    // Pads: playing copies and their progress.
    auto padsChanged = false;
    for (int s = 0; s < 16; ++s)
    {
        const auto copies = engine.getCopies (s);
        const auto progress = engine.getProgress (s);
        if (copies != shownCopies[(size_t) s] || std::abs (progress - shownProgress[(size_t) s]) > 0.002f)
        {
            shownCopies[(size_t) s] = copies;
            shownProgress[(size_t) s] = progress;
            padsChanged = true;
        }
    }

    // The map: held keys and the zones.
    std::array<uint64_t, 2> keys {};
    for (int n = 0; n < 128; ++n)
        if (engine.isKeyDown (n))
            keys[(size_t) (n / 64)] |= (uint64_t) 1 << (n & 63);
    std::vector<float> zones;
    zones.reserve (16 * 4);
    for (int s = 0; s < 16; ++s)
        for (const auto* name : { "low", "high", "root", "pitch" })
            zones.push_back (rkfx::ProcessorBase::valueOf (&slotParam (s, name)));

    if (padsChanged || zones != shownZones)
        pads.repaint();
    if (keys != shownKeys || zones != shownZones || padsChanged)
        keyMap.repaint();
    shownKeys = keys;
    shownZones = std::move (zones);
}
