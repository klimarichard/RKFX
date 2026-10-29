#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kDisplayHeight = 150;
    constexpr int kRowHeight = 152;
    constexpr int kTrackingWidth = 300;
    constexpr int kNotesWidth = 250;
    constexpr float kMeterFloorDb = -70.0f;
}

//==============================================================================
void RKFollowEditor::Display::setMeter (const FollowEngine::Meter& m, float seconds)
{
    const auto level = juce::jmax (juce::jmax (kMeterFloorDb, m.levelDb), shownLevel - 30.0f * seconds);
    const auto changed = m.note != shown.note || std::abs (m.bendCents - shown.bendCents) > 0.5f || m.velocity != shown.velocity
                      || m.expression != shown.expression || std::abs (level - shownLevel) > 0.1f;
    shown = m;
    shownLevel = level;
    if (changed)
        repaint();
}

void RKFollowEditor::Display::paint (juce::Graphics& g)
{
    using namespace rkfx;
    const auto& s = owner.settings;
    const auto accent = owner.accent();

    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::background);
    g.fillRoundedRectangle (area, Style::kPanelRadius);
    area = area.reduced (14.0f, 10.0f);

    // Left: the note, large, and the bend.
    auto noteArea = area.removeFromLeft (260.0f);
    const auto sounding = shown.note >= 0;
    g.setColour (sounding ? Palette::text : Palette::textDim);
    g.setFont (Fonts::bold (54.0f));
    g.drawText (sounding ? juce::MidiMessage::getMidiNoteName (shown.note, true, true, 3) : juce::String ("-"),
                noteArea.removeFromTop (70.0f), juce::Justification::centredLeft);

    g.setFont (Fonts::regular (12.0f));
    g.setColour (Palette::textDim);
    g.drawText (sounding ? "velocity " + juce::String (shown.velocity) + ", channel " + juce::String (s.bend == FollowEngine::Bend::mpe ? juce::String ("MPE") : juce::String (s.channel))
                         : juce::String ("no note"),
                noteArea.removeFromTop (18.0f), juce::Justification::centredLeft);

    // Bend: a needle on a -50..+50 cent scale (slides beyond it pin to the end).
    noteArea.removeFromTop (10.0f);
    auto scale = noteArea.removeFromTop (22.0f).withWidth (230.0f);
    g.setColour (Palette::outline);
    g.fillRect (scale.withSizeKeepingCentre (scale.getWidth(), 2.0f));
    for (int c = -50; c <= 50; c += 25)
    {
        const auto x = scale.getX() + scale.getWidth() * (float) (c + 50) / 100.0f;
        g.fillRect (x - 0.5f, scale.getCentreY() - (c == 0 ? 8.0f : 4.0f), 1.0f, c == 0 ? 16.0f : 8.0f);
    }
    if (sounding && s.bend != FollowEngine::Bend::off)
    {
        const auto cents = juce::jlimit (-50.0f, 50.0f, shown.bendCents);
        const auto x = scale.getX() + scale.getWidth() * (cents + 50.0f) / 100.0f;
        g.setColour (accent);
        g.fillRoundedRectangle (x - 2.0f, scale.getY(), 4.0f, scale.getHeight(), 2.0f);
        g.setColour (Palette::textDim);
        g.drawText ((shown.bendCents >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (shown.bendCents)) + " ct",
                    scale.withX (scale.getRight() + 6.0f).withWidth (60.0f), juce::Justification::centredLeft);
    }

    area.removeFromLeft (20.0f);

    // Right: level against the threshold and the expression range, and the value sent.
    auto bar = [&g] (juce::Rectangle<float> r, const juce::String& label, float fraction, juce::Colour colour, const juce::String& value)
    {
        g.setFont (Fonts::regular (12.0f));
        g.setColour (Palette::textDim);
        g.drawText (label, r.removeFromLeft (82.0f), juce::Justification::centredLeft);
        g.setColour (Palette::text);
        g.drawText (value, r.removeFromRight (70.0f), juce::Justification::centredRight);
        const auto track = r.reduced (4.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 8.0f, 10.0f);
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * juce::jlimit (0.0f, 1.0f, fraction)), 3.0f);
        return track;
    };

    const auto rowHeight = area.getHeight() / 3.0f;
    auto levelFraction = [] (float db) { return (db - kMeterFloorDb) / -kMeterFloorDb; };

    const auto level = bar (area.removeFromTop (rowHeight), "Level", levelFraction (shownLevel), accent.withAlpha (0.6f),
                            juce::String (shownLevel, 1) + " dB");
    auto mark = [&] (float db, juce::Colour colour)
    {
        const auto x = level.getX() + level.getWidth() * juce::jlimit (0.0f, 1.0f, levelFraction (db));
        g.setColour (colour);
        g.fillRect (x - 1.0f, level.getY() - 5.0f, 2.0f, level.getHeight() + 10.0f);
    };
    mark (s.thresholdDb, Palette::text);
    if (s.expression)
    {
        mark (s.floorDb, Palette::textDim);
        mark (s.ceilingDb, Palette::textDim);
    }

    const auto controller = s.controller >= FollowEngine::kPressure ? juce::String ("Pressure") : "CC " + juce::String (s.controller);
    bar (area.removeFromTop (rowHeight), s.expression ? controller : juce::String ("Expression off"),
         shown.expression < 0 ? 0.0f : (float) shown.expression / 127.0f, accent,
         shown.expression < 0 || ! s.expression ? juce::String ("-") : juce::String (shown.expression));

    g.setFont (Fonts::regular (11.0f));
    g.setColour (Palette::textDim);
    g.drawText ("White mark: Threshold" + juce::String (s.expression ? ", gray marks: expression Floor and Ceiling" : ""),
                area, juce::Justification::centredLeft);
}

//==============================================================================
RKFollowEditor::RKFollowEditor (RKFollowProcessor& p)
    : rkfx::EditorBase (p, 1000, 630),
      follow (p),
      settings (p.currentSettings()),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    notes        = std::make_unique<rkfx::ToggleSwitch> (param ("notes"), accent(), "Notes");
    expr         = std::make_unique<rkfx::ToggleSwitch> (param ("expr"), accent(), "On");
    range        = std::make_unique<rkfx::SegmentedSwitch> (param ("range"), accent());
    bend         = std::make_unique<rkfx::SegmentedSwitch> (param ("bend"), accent());
    threshold    = std::make_unique<Knob> (param ("threshold"), "Threshold");
    transpose    = std::make_unique<Knob> (param ("transpose"), "Transpose");
    velMin       = std::make_unique<Knob> (param ("vel_min"), "Velocity Min");
    velMax       = std::make_unique<Knob> (param ("vel_max"), "Velocity Max");
    retrigger    = std::make_unique<Knob> (param ("retrigger"), "Retrigger");
    bendRange    = std::make_unique<Knob> (param ("bend_range"), "Bend Range");
    glide        = std::make_unique<Knob> (param ("glide"), "Glide");
    channel      = std::make_unique<Knob> (param ("channel"), "Channel");
    exprCc       = std::make_unique<Knob> (param ("expr_cc"), "Controller");
    exprFloor    = std::make_unique<Knob> (param ("expr_floor"), "Floor");
    exprCeiling  = std::make_unique<Knob> (param ("expr_ceiling"), "Ceiling");
    exprMin      = std::make_unique<Knob> (param ("expr_min"), "Min");
    exprMax      = std::make_unique<Knob> (param ("expr_max"), "Max");
    exprCurve    = std::make_unique<Knob> (param ("expr_curve"), "Curve");
    exprResponse = std::make_unique<Knob> (param ("expr_response"), "Response");

    addToCanvas (display);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             notes.get(), expr.get(), range.get(), bend.get(), threshold.get(), transpose.get(), velMin.get(), velMax.get(),
             retrigger.get(), bendRange.get(), glide.get(), channel.get(), exprCc.get(), exprFloor.get(), exprCeiling.get(),
             exprMin.get(), exprMax.get(), exprCurve.get(), exprResponse.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
    meterTimer.startTimerHz (30);
}

void RKFollowEditor::onParameterChange()
{
    settings = follow.currentSettings();

    for (auto* k : { velMin.get(), velMax.get(), retrigger.get(), transpose.get() })
        k->setDimmed (! settings.notes);
    for (auto* k : { bendRange.get(), glide.get() })
        k->setDimmed (! settings.notes || settings.bend == FollowEngine::Bend::off);
    channel->setDimmed (settings.bend == FollowEngine::Bend::mpe);
    for (auto* k : { exprCc.get(), exprFloor.get(), exprCeiling.get(), exprMin.get(), exprMax.get(), exprCurve.get(), exprResponse.get() })
        k->setDimmed (! settings.expression);

    display.repaint();
}

void RKFollowEditor::updateMeters()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = lastMeterTime > 0.0 ? (float) (now - lastMeterTime) : 0.0f;
    lastMeterTime = now;
    display.setMeter (follow.takeMeter(), elapsed);
}

juce::Rectangle<int> RKFollowEditor::displayArea() const    { return contentArea().withHeight (kDisplayHeight); }
juce::Rectangle<int> RKFollowEditor::trackingArea() const   { return contentArea().withTrimmedTop (kDisplayHeight + kGap).withHeight (kRowHeight).withWidth (kTrackingWidth); }
juce::Rectangle<int> RKFollowEditor::notesArea() const      { return trackingArea().withX (trackingArea().getRight() + kGap).withWidth (kNotesWidth); }
juce::Rectangle<int> RKFollowEditor::bendArea() const
{
    const auto left = notesArea().getRight() + kGap;
    return trackingArea().withX (left).withWidth (contentArea().getRight() - left);
}
juce::Rectangle<int> RKFollowEditor::expressionArea() const { return contentArea().withTrimmedTop (kDisplayHeight + kRowHeight + 2 * kGap); }

void RKFollowEditor::layoutContent()
{
    using rkfx::Knob;
    display.setBounds (displayArea());

    auto knobRow = [] (juce::Rectangle<int> row, std::initializer_list<Knob*> knobs)
    {
        const auto width = row.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
            k->setBounds (row.removeFromLeft (width));
    };

    {
        auto area = trackingArea().reduced (8, 0);
        auto title = area.removeFromTop (kTitleHeight + 4);
        notes->setBounds (title.removeFromRight (70).withSizeKeepingCentre (70, 22));
        range->setBounds (area.removeFromTop (26));
        knobRow (area.removeFromTop (Knob::kDefaultHeight + 4).withTrimmedTop (4), { threshold.get(), transpose.get() });
    }
    {
        auto area = notesArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 30);
        knobRow (area.removeFromTop (Knob::kDefaultHeight + 4).withTrimmedTop (4), { velMin.get(), velMax.get(), retrigger.get() });
    }
    {
        auto area = bendArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 4);
        bend->setBounds (area.removeFromTop (26).withWidth (240));
        knobRow (area.removeFromTop (Knob::kDefaultHeight + 4).withTrimmedTop (4), { bendRange.get(), glide.get(), channel.get() });
    }
    {
        auto area = expressionArea().reduced (8, 0);
        auto title = area.removeFromTop (kTitleHeight);
        expr->setBounds (title.removeFromRight (50).withSizeKeepingCentre (50, 22));
        knobRow (area.removeFromTop (Knob::kDefaultHeight), { exprCc.get(), exprFloor.get(), exprCeiling.get(), exprMin.get(),
                                                              exprMax.get(), exprCurve.get(), exprResponse.get() });
    }
}

void RKFollowEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, trackingArea(), "Tracking");
    drawSection (g, notesArea(), "Notes");
    drawSection (g, bendArea(), "Pitch Bend");
    drawSection (g, expressionArea(), "Expression");
}
