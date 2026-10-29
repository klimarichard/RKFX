#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kGap = 8;
    constexpr int kTitleHeight = 26;
    constexpr int kDisplayHeight = 150;
    constexpr int kTabsHeight = 32;
    constexpr int kRowHeight = 124;
    constexpr int kVelocityWidth = 500;
    constexpr float kFloorDb = -60.0f;
}

//==============================================================================
void RKTriggerEditor::HitDisplay::push (int t, float levelDb, int newHits, int velocity)
{
    history[(size_t) t][(size_t) next] = { levelDb, newHits > 0 ? velocity : 0 };
}

void RKTriggerEditor::HitDisplay::paint (juce::Graphics& g)
{
    using namespace rkfx;
    const auto accent = owner.accent();
    const auto dual = owner.settings.mode == TriggerEngine::Mode::dual;

    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::background);
    g.fillRoundedRectangle (area, Style::kPanelRadius);
    area = area.reduced (8.0f, 6.0f);

    const auto laneHeight = (area.getHeight() - 6.0f) / 2.0f;
    for (int t = 0; t < TriggerParams::kTriggers; ++t)
    {
        auto lane = area.removeFromTop (laneHeight);
        area.removeFromTop (6.0f);
        const auto active = t == 0 || dual;
        const auto& tr = owner.settings.triggers[(size_t) t];

        auto label = lane.removeFromLeft (110.0f);
        g.setFont (Fonts::bold (13.0f));
        g.setColour (active ? Palette::text : Palette::textDim);
        g.drawText ("Trigger " + juce::String (t + 1), label.removeFromTop (20.0f), juce::Justification::centredLeft);
        g.setFont (Fonts::regular (11.0f));
        g.setColour (Palette::textDim);
        g.drawText (active ? juce::MidiMessage::getMidiNoteName (tr.note, true, true, 3) + ", ch " + juce::String (tr.channel)
                           : juce::String ("off (Mono)"),
                    label.removeFromTop (16.0f), juce::Justification::centredLeft);

        g.setColour (Palette::panel.withAlpha (0.6f));
        g.fillRoundedRectangle (lane, 3.0f);
        if (! active)
            continue;

        auto yFor = [&lane] (float db) { return lane.getBottom() - lane.getHeight() * juce::jlimit (0.0f, 1.0f, (db - kFloorDb) / -kFloorDb); };
        const auto column = lane.getWidth() / (float) kPoints;

        for (int age = 0; age < kPoints; ++age)
        {
            const auto& p = history[(size_t) t][(size_t) ((next + age) % kPoints)];   // next: the oldest
            const auto x = lane.getX() + column * (float) age;
            if (p.levelDb > kFloorDb)
            {
                const auto y = yFor (p.levelDb);
                g.setColour (accent.withAlpha (0.25f));
                g.fillRect (x, y, column + 0.5f, lane.getBottom() - y);
            }
            if (p.velocity > 0)
            {
                const auto h = lane.getHeight() * (float) p.velocity / 127.0f;
                g.setColour (accent);
                g.fillRect (x, lane.getBottom() - h, juce::jmax (2.0f, column), h);
            }
        }

        // The threshold.
        const auto y = yFor (tr.thresholdDb);
        g.setColour (Palette::text);
        g.fillRect (lane.getX(), y - 0.5f, lane.getWidth(), 1.0f);
    }
}

//==============================================================================
void RKTriggerEditor::Tabs::paint (juce::Graphics& g)
{
    using namespace rkfx;
    const auto dual = owner.settings.mode == TriggerEngine::Mode::dual;
    auto area = getLocalBounds().toFloat();
    const auto width = area.getWidth() / 2.0f;

    for (int t = 0; t < TriggerParams::kTriggers; ++t)
    {
        const auto tab = area.removeFromLeft (width).reduced (t == 0 ? 0.0f : 4.0f, 0.0f).withTrimmedRight (t == 0 ? 4.0f : 0.0f);
        const auto isSelected = owner.selected() == t;
        g.setColour (owner.panelColour().brighter (isSelected ? 0.12f : 0.0f));
        g.fillRoundedRectangle (tab, Style::kPanelRadius);
        if (isSelected)
        {
            g.setColour (owner.accent().withAlpha (0.7f));
            g.drawRoundedRectangle (tab.reduced (0.75f), Style::kPanelRadius, 1.5f);
        }
        g.setColour (t == 0 || dual ? Palette::text : Palette::textDim);
        g.setFont (Fonts::bold (13.0f));
        g.drawText ("Trigger " + juce::String (t + 1) + (t == 1 && ! dual ? "  (Dual mode only)" : ""), tab, juce::Justification::centred);
    }
}

void RKTriggerEditor::Tabs::mouseDown (const juce::MouseEvent& e)
{
    owner.select (e.x < getWidth() / 2 ? 0 : 1);
}

//==============================================================================
RKTriggerEditor::RKTriggerEditor (RKTriggerProcessor& p)
    : rkfx::EditorBase (p, 900, 640),
      trigger (p),
      settings (p.currentSettings()),
      watcher (p, [this] { onParameterChange(); })
{
    using rkfx::Knob;
    using TriggerParams::id;
    auto param = [&p] (const juce::String& paramId) -> juce::RangedAudioParameter& { return *p.findParam (paramId); };

    addToCanvas (display);
    addToCanvas (tabs);

    for (int t = 0; t < TriggerParams::kTriggers; ++t)
    {
        auto& c = controls[(size_t) t];
        c.threshold = std::make_unique<Knob> (param (id (t, "threshold")), "Threshold");
        c.hpf       = std::make_unique<Knob> (param (id (t, "hpf")), "High-Pass");
        c.lpf       = std::make_unique<Knob> (param (id (t, "lpf")), "Low-Pass");
        c.scan      = std::make_unique<Knob> (param (id (t, "scan")), "Scan");
        c.mask      = std::make_unique<Knob> (param (id (t, "mask")), "Mask");
        c.retrigger = std::make_unique<Knob> (param (id (t, "retrigger")), "Retrigger");
        c.dynamics  = std::make_unique<Knob> (param (id (t, "dynamics")), "Dynamics");
        c.curve     = std::make_unique<Knob> (param (id (t, "curve")), "Curve");
        c.velMin    = std::make_unique<Knob> (param (id (t, "vel_min")), "Velocity Min");
        c.velMax    = std::make_unique<Knob> (param (id (t, "vel_max")), "Velocity Max");
        c.note      = std::make_unique<Knob> (param (id (t, "note")), "Note");
        c.channel   = std::make_unique<Knob> (param (id (t, "channel")), "Channel");
        c.length    = std::make_unique<Knob> (param (id (t, "length")), "Length");

        for (juce::Component* k : std::initializer_list<juce::Component*> {
                 c.threshold.get(), c.hpf.get(), c.lpf.get(), c.scan.get(), c.mask.get(), c.retrigger.get(), c.dynamics.get(),
                 c.curve.get(), c.velMin.get(), c.velMax.get(), c.note.get(), c.channel.get(), c.length.get() })
            addToCanvas (*k);
    }

    mode      = std::make_unique<rkfx::SegmentedSwitch> (param ("mode"), accent(), juce::StringArray {}, "Mode");
    audio     = std::make_unique<rkfx::SegmentedSwitch> (param ("audio"), accent(), juce::StringArray {}, "Audio");
    crosstalk = std::make_unique<Knob> (param ("crosstalk"), "Crosstalk");
    for (juce::Component* c : std::initializer_list<juce::Component*> { mode.get(), audio.get(), crosstalk.get() })
        addToCanvas (*c);

    onParameterChange();
    finishSetup();
    meterTimer.startTimerHz (30);
}

int RKTriggerEditor::selected() const
{
    return juce::jlimit (0, TriggerParams::kTriggers - 1, trigger.selectedTrigger.load());
}

void RKTriggerEditor::select (int t)
{
    trigger.selectedTrigger = t;
    for (int i = 0; i < TriggerParams::kTriggers; ++i)
    {
        auto& c = controls[(size_t) i];
        for (juce::Component* k : std::initializer_list<juce::Component*> {
                 c.threshold.get(), c.hpf.get(), c.lpf.get(), c.scan.get(), c.mask.get(), c.retrigger.get(), c.dynamics.get(),
                 c.curve.get(), c.velMin.get(), c.velMax.get(), c.note.get(), c.channel.get(), c.length.get() })
            k->setVisible (i == t);
    }
    tabs.repaint();
    repaint();
}

void RKTriggerEditor::onParameterChange()
{
    settings = trigger.currentSettings();
    const auto dual = settings.mode == TriggerEngine::Mode::dual;

    auto& second = controls[1];
    for (auto* k : { second.threshold.get(), second.hpf.get(), second.lpf.get(), second.scan.get(), second.mask.get(),
                     second.retrigger.get(), second.dynamics.get(), second.curve.get(), second.velMin.get(), second.velMax.get(),
                     second.note.get(), second.channel.get(), second.length.get() })
        k->setDimmed (! dual);
    crosstalk->setDimmed (! dual);

    display.repaint();
    select (selected());
}

void RKTriggerEditor::updateMeters()
{
    for (int t = 0; t < TriggerParams::kTriggers; ++t)
    {
        const auto m = trigger.takeMeter (t);
        display.push (t, m.levelDb, m.hits - lastHits[(size_t) t], m.velocity);
        lastHits[(size_t) t] = m.hits;
    }
    display.next = (display.next + 1) % HitDisplay::kPoints;
    display.repaint();
}

juce::Rectangle<int> RKTriggerEditor::displayArea() const  { return contentArea().withHeight (kDisplayHeight); }
juce::Rectangle<int> RKTriggerEditor::tabsArea() const     { return contentArea().withTrimmedTop (kDisplayHeight + kGap).withHeight (kTabsHeight); }
juce::Rectangle<int> RKTriggerEditor::detectionArea() const
{
    return contentArea().withTrimmedTop (kDisplayHeight + kTabsHeight + 2 * kGap).withHeight (kRowHeight);
}
juce::Rectangle<int> RKTriggerEditor::velocityArea() const
{
    return contentArea().withTrimmedTop (kDisplayHeight + kTabsHeight + kRowHeight + 3 * kGap).withHeight (kRowHeight).withWidth (kVelocityWidth);
}
juce::Rectangle<int> RKTriggerEditor::noteArea() const
{
    return velocityArea().withX (velocityArea().getRight() + kGap).withRight (contentArea().getRight());
}

void RKTriggerEditor::layoutContent()
{
    using rkfx::Knob;
    display.setBounds (displayArea());
    tabs.setBounds (tabsArea());

    auto knobRow = [] (juce::Rectangle<int> area, std::initializer_list<Knob*> knobs)
    {
        area = area.reduced (8, 0).withTrimmedTop (kTitleHeight).withHeight (Knob::kDefaultHeight);
        const auto width = area.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
            k->setBounds (area.removeFromLeft (width));
    };

    for (auto& c : controls)
    {
        knobRow (detectionArea(), { c.threshold.get(), c.hpf.get(), c.lpf.get(), c.scan.get(), c.mask.get(), c.retrigger.get() });
        knobRow (velocityArea(), { c.dynamics.get(), c.curve.get(), c.velMin.get(), c.velMax.get() });
        knobRow (noteArea(), { c.note.get(), c.channel.get(), c.length.get() });
    }

    auto global = globalArea();
    mode->setBounds (global.removeFromLeft (150).withSizeKeepingCentre (150, 44));
    global.removeFromLeft (12);
    crosstalk->setBounds (global.removeFromLeft (Knob::kDefaultWidth + 8).withSizeKeepingCentre (Knob::kDefaultWidth + 8, Knob::kDefaultHeight));
    global.removeFromLeft (12);
    audio->setBounds (global.removeFromLeft (150).withSizeKeepingCentre (150, 44));
}

void RKTriggerEditor::paintContent (juce::Graphics& g)
{
    const auto name = "Trigger " + juce::String (selected() + 1) + " ";
    drawSection (g, detectionArea(), name + "Detection");
    drawSection (g, velocityArea(), "Velocity");
    drawSection (g, noteArea(), "Note");
}
