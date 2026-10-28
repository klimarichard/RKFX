#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kStatusHeight = 64;
    constexpr int kSwitchHeight = 56;

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

RKLoopEditor::RKLoopEditor (RKLoopProcessor& p)
    : rkfx::EditorBase (p, 640, 380), loopProcessor (p)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    const char* const labels[] = { "Record", "Stop", "Undo", "Clear" };
    for (int b = 0; b < 4; ++b)
    {
        footswitches[(size_t) b] = std::make_unique<Footswitch> (param (LoopParams::buttonIds[b]), labels[b]);
        addToCanvas (*footswitches[(size_t) b]);
    }

    reverse   = std::make_unique<rkfx::ToggleSwitch> (param ("reverse"), accent(), "Reverse");
    halfSpeed = std::make_unique<rkfx::ToggleSwitch> (param ("half_speed"), accent(), "Half Speed");
    level     = std::make_unique<Knob> (param ("level"), "Loop Level");
    for (juce::Component* c : std::initializer_list<juce::Component*> { reverse.get(), halfSpeed.get(), level.get() })
        addToCanvas (*c);

    finishSetup();
    refresher.startTimerHz (30);
}

juce::Rectangle<int> RKLoopEditor::loopArea() const { return contentArea(); }

juce::Rectangle<int> RKLoopEditor::statusArea() const
{
    return loopArea().reduced (8, 0).withTrimmedTop (kTitleHeight).removeFromTop (kStatusHeight);
}

void RKLoopEditor::layoutContent()
{
    using rkfx::Knob;

    auto area = loopArea().reduced (8, 0);
    area.removeFromTop (kTitleHeight + kStatusHeight + 10);

    auto right = area.removeFromRight (130);
    level->setBounds (right.removeFromTop (Knob::kDefaultHeight));

    auto switches = area.removeFromTop (kSwitchHeight);
    const auto width = (switches.getWidth() - 3 * 8) / 4;
    for (auto& f : footswitches)
    {
        f->setBounds (switches.removeFromLeft (width));
        switches.removeFromLeft (8);
    }

    area.removeFromTop (12);
    auto toggles = area.removeFromTop (28);
    reverse->setBounds (toggles.removeFromLeft (100));
    toggles.removeFromLeft (8);
    halfSpeed->setBounds (toggles.removeFromLeft (110));
}

void RKLoopEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, loopArea(), "Loop");

    const auto& engine = loopProcessor.getEngine();
    const auto state = engine.getState();
    auto area = statusArea();

    // State, and the time: position / length (or the recording so far).
    const auto rate = engine.getMaxLength() / LoopEngine::kMaxSeconds;
    const auto length = engine.getRecordedLength();
    auto top = area.removeFromTop (30);
    g.setColour (state == LoopEngine::State::recording || state == LoopEngine::State::overdubbing ? rkfx::Palette::warning
                                                                                                  : rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (18.0f));
    g.drawText (stateName (state), top, juce::Justification::centredLeft);

    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (14.0f));
    juce::String time;
    if (rate > 0.0 && state != LoopEngine::State::empty)
        time = state == LoopEngine::State::recording
                   ? juce::String (length / rate, 1) + " s"
                   : juce::String (engine.getPosition() / rate, 1) + " / " + juce::String (length / rate, 1) + " s";
    if (engine.canRedo())
        time << "   (undone)";
    g.drawText (time, top, juce::Justification::centredRight);

    // Progress through the loop.
    auto bar = area.removeFromTop (10).toFloat();
    g.setColour (rkfx::Palette::knobTrack);
    g.fillRoundedRectangle (bar, 3.0f);
    if (length > 0 && state != LoopEngine::State::recording)
    {
        g.setColour (accent());
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) (engine.getPosition() / length)), 3.0f);
    }
}

void RKLoopEditor::refresh()
{
    const auto& engine = loopProcessor.getEngine();
    const auto state = engine.getState();

    // The Record footswitch says what its next press does.
    juce::String next;
    switch (state)
    {
        case LoopEngine::State::empty:       next = "Record"; break;
        case LoopEngine::State::recording:   next = "Play"; break;
        case LoopEngine::State::playing:     next = "Overdub"; break;
        case LoopEngine::State::overdubbing: next = "Play"; break;
        case LoopEngine::State::stopped:     next = "Play"; break;
    }
    footswitches[0]->setButtonText (next);
    footswitches[2]->setButtonText (engine.canRedo() ? "Redo" : "Undo");

    repaint (statusArea());
}
