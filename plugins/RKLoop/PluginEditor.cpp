#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kTileHeight = 64;
    constexpr int kStatusHeight = 26;
    constexpr int kSwitchHeight = 48;

    juce::String stateName (LoopEngine::State s)
    {
        switch (s)
        {
            case LoopEngine::State::empty:       return "Empty";
            case LoopEngine::State::recording:   return "Recording";
            case LoopEngine::State::playing:     return "Playing";
            case LoopEngine::State::overdubbing: return "Overdubbing";
            case LoopEngine::State::stopped:     return "Stopped";
        }
        return {};
    }
}

//==============================================================================
RKLoopEditor::Footswitch::Footswitch (juce::RangedAudioParameter& p, const juce::String& text)
    : juce::TextButton (text), param (p)
{
    rkfx::tagParameter (*this, p.getParameterID());   // RKBoard: right-click to assign it
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

// A right-click belongs to the host (RKBoard's assign menu), not the switch.
void RKLoopEditor::Footswitch::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseDown (e);
}

void RKLoopEditor::Footswitch::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseUp (e);
}

//==============================================================================
RKLoopEditor::Tile::Tile (RKLoopEditor& o, int i) : owner (o), index (i)
{
}

void RKLoopEditor::Tile::paint (juce::Graphics& g)
{
    const auto& engine = owner.loopProcessor.getEngine();
    const auto state = engine.getState (index);
    const auto selected = engine.getSelected() == index;
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    // Body: brighter when it sounds, warning colour while it records.
    const auto sounding = state == LoopEngine::State::playing || state == LoopEngine::State::overdubbing;
    const auto writing = state == LoopEngine::State::recording || state == LoopEngine::State::overdubbing;
    auto body = rkfx::Palette::knobBody;
    if (sounding)
        body = body.interpolatedWith (owner.accent(), 0.35f);
    if (writing)
        body = body.interpolatedWith (rkfx::Palette::warning, 0.45f);
    g.setColour (body);
    g.fillRoundedRectangle (bounds, 4.0f);

    // Queued (waiting for the downbeat or the end of the part): a blinking outline.
    if (selected || (engine.isQueued (index) && owner.blink))
    {
        g.setColour (engine.isQueued (index) ? rkfx::Palette::warning : owner.accent());
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 2.0f);
    }

    auto area = getLocalBounds().reduced (6, 4);
    auto top = area.removeFromTop (20);
    g.setColour (state == LoopEngine::State::empty ? rkfx::Palette::textDim : rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (15.0f));
    g.drawText (juce::String (index + 1), top, juce::Justification::centredLeft);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (11.0f));
    const auto isLayer = owner.loopProcessor.findParam (LoopParams::roleId (index))->getValue() >= 0.5f;   // the setting, even before audio runs
    g.drawText (isLayer ? "Layer" : "Part", top, juce::Justification::centredRight);

    // Length, and the position through the loop.
    const auto rate = engine.getMaxLength() / LoopEngine::kMaxSeconds;
    const auto length = engine.getRecordedLength (index);
    if (rate > 0.0 && length > 0)
    {
        g.drawText (juce::String (length / rate, 1) + " s", area.removeFromTop (18), juce::Justification::centredLeft);
        auto bar = area.removeFromBottom (6).toFloat();
        g.setColour (rkfx::Palette::knobTrack);
        g.fillRoundedRectangle (bar, 2.0f);
        if (sounding && engine.getLength (index) > 0)
        {
            g.setColour (owner.accent());
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) (engine.getPosition (index) / engine.getLength (index))), 2.0f);
        }
    }
}

void RKLoopEditor::Tile::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    auto& loop = *owner.loopProcessor.findParam ("loop");
    loop.beginChangeGesture();
    loop.setValueNotifyingHost (loop.convertTo0to1 ((float) (index + 1)));
    loop.endChangeGesture();
}

//==============================================================================
RKLoopEditor::RKLoopEditor (RKLoopProcessor& p)
    : rkfx::EditorBase (p, 760, 450), loopProcessor (p)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    const char* const labels[] = { "Record", "Stop", "Undo", "Clear", "Next", "Previous" };
    for (int b = 0; b < 6; ++b)
    {
        footswitches[(size_t) b] = std::make_unique<Footswitch> (param (LoopParams::buttonIds[b]), labels[b]);
        addToCanvas (*footswitches[(size_t) b]);
    }
    footswitches[4]->setButtonText (juce::String::fromUTF8 ("Next \xe2\x96\xb6"));
    footswitches[5]->setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80 Prev"));

    for (int i = 0; i < 8; ++i)
    {
        tiles[(size_t) i] = std::make_unique<Tile> (*this, i);
        addToCanvas (*tiles[(size_t) i]);
    }

    sync      = std::make_unique<rkfx::ToggleSwitch> (param ("sync"), accent(), "Sync");
    reverse   = std::make_unique<rkfx::ToggleSwitch> (param ("reverse"), accent(), "Reverse");
    halfSpeed = std::make_unique<rkfx::ToggleSwitch> (param ("half_speed"), accent(), "Half Speed");
    level     = std::make_unique<Knob> (param ("level"), "Loop Level");
    for (juce::Component* c : std::initializer_list<juce::Component*> { sync.get(), reverse.get(), halfSpeed.get(), level.get() })
        addToCanvas (*c);

    showRoleSwitch (p.getEngine().getSelected());
    finishSetup();
    refresher.startTimerHz (30);
}

void RKLoopEditor::showRoleSwitch (int loop)
{
    roleFor = loop;
    if (role != nullptr)
        removeChildComponent (role.get());
    role = std::make_unique<rkfx::SegmentedSwitch> (*loopProcessor.findParam (LoopParams::roleId (loop)), accent(),
                                                    juce::StringArray { "Part", "Layer" });
    addToCanvas (*role);
    resized();
}

juce::Rectangle<int> RKLoopEditor::loopsArea() const   { return contentArea().removeFromTop (kTitleHeight + kTileHeight + 12); }
juce::Rectangle<int> RKLoopEditor::controlArea() const { return contentArea().withTrimmedTop (kTitleHeight + kTileHeight + 12 + 10); }

juce::Rectangle<int> RKLoopEditor::statusArea() const
{
    return controlArea().reduced (8, 0).withTrimmedTop (kTitleHeight).removeFromTop (kStatusHeight);
}

void RKLoopEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = loopsArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        auto row = area.removeFromTop (kTileHeight);
        const auto width = (row.getWidth() - 7 * 6) / 8;
        for (auto& t : tiles)
        {
            t->setBounds (row.removeFromLeft (width));
            row.removeFromLeft (6);
        }
    }

    {
        auto area = controlArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + kStatusHeight + 6);

        auto right = area.removeFromRight (120);
        level->setBounds (right.removeFromTop (Knob::kDefaultHeight));

        // Record, Stop, Undo, Clear, then Previous before Next.
        auto switches = area.removeFromTop (kSwitchHeight);
        const auto width = (switches.getWidth() - 5 * 6) / 6;
        for (const auto b : { 0, 1, 2, 3, 5, 4 })
        {
            footswitches[(size_t) b]->setBounds (switches.removeFromLeft (width));
            switches.removeFromLeft (6);
        }

        area.removeFromTop (10);
        auto toggles = area.removeFromTop (28);
        if (role != nullptr)
            role->setBounds (toggles.removeFromLeft (150));
        toggles.removeFromLeft (16);
        sync->setBounds (toggles.removeFromLeft (70));
        toggles.removeFromLeft (8);
        reverse->setBounds (toggles.removeFromLeft (90));
        toggles.removeFromLeft (8);
        halfSpeed->setBounds (toggles.removeFromLeft (110));
    }
}

void RKLoopEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, loopsArea(), "Loops");
    drawSection (g, controlArea(), "Selected loop");

    // The selected loop's state and time.
    const auto& engine = loopProcessor.getEngine();
    const auto i = engine.getSelected();
    const auto state = engine.getState (i);
    auto area = statusArea();

    juce::String text = "Loop " + juce::String (i + 1) + ": " + stateName (state);
    if (engine.isQueued (i))
        text << (state == LoopEngine::State::empty ? " (records on the downbeat)" : " (starts at the end of the loop)");
    if (engine.canRedo (i))
        text << "   (undone)";

    g.setColour (state == LoopEngine::State::recording || state == LoopEngine::State::overdubbing ? rkfx::Palette::warning
                                                                                                  : rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (15.0f));
    g.drawText (text, area, juce::Justification::centredLeft);

    const auto rate = engine.getMaxLength() / LoopEngine::kMaxSeconds;
    const auto length = engine.getRecordedLength (i);
    if (rate > 0.0 && state != LoopEngine::State::empty)
    {
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (14.0f));
        g.drawText (state == LoopEngine::State::recording
                        ? juce::String (length / rate, 1) + " s"
                        : juce::String (engine.getPosition (i) / rate, 1) + " / " + juce::String (length / rate, 1) + " s",
                    area, juce::Justification::centredRight);
    }
}

void RKLoopEditor::refresh()
{
    const auto& engine = loopProcessor.getEngine();
    const auto selected = engine.getSelected();
    if (selected != roleFor)
        showRoleSwitch (selected);

    // The Record footswitch says what its next press does.
    juce::String next;
    switch (engine.getState (selected))
    {
        case LoopEngine::State::empty:       next = "Record"; break;
        case LoopEngine::State::recording:   next = "Play"; break;
        case LoopEngine::State::playing:     next = "Overdub"; break;
        case LoopEngine::State::overdubbing: next = "Play"; break;
        case LoopEngine::State::stopped:     next = "Play"; break;
    }
    footswitches[0]->setButtonText (next);
    footswitches[2]->setButtonText (engine.canRedo (selected) ? "Redo" : "Undo");

    if (++blinkCounter >= 8)
    {
        blinkCounter = 0;
        blink = ! blink;
    }

    repaint (statusArea());
    for (auto& t : tiles)
        t->repaint();
}
