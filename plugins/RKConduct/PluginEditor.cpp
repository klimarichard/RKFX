#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kSongWidth = 360;
    constexpr int kGap = 10;
    constexpr int kRowHeight = 24;
}

//==============================================================================
RKConductEditor::Footswitch::Footswitch (juce::RangedAudioParameter& p, const juce::String& text)
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

void RKConductEditor::Footswitch::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseDown (e);
}

void RKConductEditor::Footswitch::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        juce::TextButton::mouseUp (e);
}

//==============================================================================
RKConductEditor::TrackRow::TrackRow (RKConductProcessor& p, int t) : processor (p), track (t)
{
    on.setToggleState (processor.isTrackOn (track), juce::dontSendNotification);
    on.onClick = [this] { processor.setTrackOn (track, on.getToggleState()); repaint(); };
    addAndMakeVisible (on);

    channel.addItem ("As recorded", 1);
    for (int ch = 1; ch <= 16; ++ch)
        channel.addItem ("Channel " + juce::String (ch), ch + 1);
    channel.setSelectedId (processor.getTrackChannel (track) + 1, juce::dontSendNotification);
    channel.onChange = [this] { processor.setTrackChannel (track, channel.getSelectedId() - 1); };
    addAndMakeVisible (channel);
}

void RKConductEditor::TrackRow::resized()
{
    auto r = getLocalBounds();
    on.setBounds (r.removeFromLeft (26));
    channel.setBounds (r.removeFromRight (120).reduced (0, 1));
}

void RKConductEditor::TrackRow::paint (juce::Graphics& g)
{
    const auto& info = processor.getTracks()[(size_t) track];
    auto r = getLocalBounds().withTrimmedLeft (28).withTrimmedRight (126);
    g.setColour (processor.isTrackOn (track) ? rkfx::Palette::text : rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (13.0f));
    g.drawText (info.name, r, juce::Justification::centredLeft, true);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (11.5f));
    g.drawText (juce::String (info.notes) + " notes", r, juce::Justification::centredRight);
}

//==============================================================================
RKConductEditor::RKConductEditor (RKConductProcessor& p)
    : rkfx::EditorBase (p, 780, 560), conductor (p)
{
    using rkfx::Knob;
    auto param = [&p] (const char* id) -> juce::RangedAudioParameter& { return *p.findParam (id); };

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a MIDI file", juce::File(), "*.mid;*.midi;*.smf");
        juce::Component::SafePointer<RKConductEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr || fc.getResult() == juce::File())
                                      return;
                                  safe->note = safe->conductor.loadFile (fc.getResult());
                                  safe->repaint();
                              });
    };
    addToCanvas (loadButton);

    tap       = std::make_unique<Footswitch> (param ("tap"), "Tap");
    stop      = std::make_unique<Footswitch> (param ("stop"), "Stop");
    tapEvery  = std::make_unique<Knob> (param ("tap_every"), "Tap Every");
    tempo     = std::make_unique<Knob> (param ("tempo"), "Tempo");
    countIn   = std::make_unique<Knob> (param ("count_in"), "Count-in");
    key1      = std::make_unique<Knob> (param ("key1"), "Tap Key 1");
    key2      = std::make_unique<Knob> (param ("key2"), "Tap Key 2");
    tapCC     = std::make_unique<Knob> (param ("tap_cc"), "Tap CC");
    fileTempo = std::make_unique<rkfx::ToggleSwitch> (param ("file_tempo"), accent(), "File Tempo");
    loop      = std::make_unique<rkfx::ToggleSwitch> (param ("loop"), accent(), "Loop");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             tap.get(), stop.get(), tapEvery.get(), tempo.get(), countIn.get(), key1.get(), key2.get(), tapCC.get(),
             fileTempo.get(), loop.get() })
        addToCanvas (*c);

    tempoWatcher = std::make_unique<juce::ParameterAttachment> (param ("file_tempo"), [this] (float v) { tempo->setDimmed (v >= 0.5f); });
    tempoWatcher->sendInitialUpdate();

    finishSetup();
    refresh();
    refresher.startTimerHz (20);
}

juce::Rectangle<int> RKConductEditor::songArea() const    { return contentArea().withWidth (kSongWidth); }
juce::Rectangle<int> RKConductEditor::conductArea() const { return contentArea().withTrimmedLeft (kSongWidth + kGap); }

juce::Rectangle<int> RKConductEditor::statusArea() const
{
    return conductArea().reduced (8, 0).withTrimmedTop (kTitleHeight).removeFromTop (44);
}

void RKConductEditor::rebuildRows()
{
    rows.clear();   // a component leaves its parent (the canvas) as it goes
    for (int t = 0; t < (int) conductor.getTracks().size(); ++t)
    {
        rows.push_back (std::make_unique<TrackRow> (conductor, t));
        addToCanvas (*rows.back());
    }
    layoutContent();
    repaint();
}

void RKConductEditor::layoutContent()
{
    using rkfx::Knob;

    {
        auto area = songArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight);
        loadButton.setBounds (area.removeFromTop (28).removeFromLeft (150));
        area.removeFromTop (36);   // the song's name and length, painted
        for (auto& row : rows)
            row->setBounds (area.removeFromTop (kRowHeight));
    }

    {
        auto area = conductArea().reduced (8, 0);
        area.removeFromTop (kTitleHeight + 44 + 6);   // the status, painted
        auto buttons = area.removeFromTop (48);
        tap->setBounds (buttons.removeFromLeft (buttons.getWidth() * 2 / 3).reduced (0, 0).withTrimmedRight (6));
        stop->setBounds (buttons);
        area.removeFromTop (10);

        auto row1 = area.removeFromTop (Knob::kDefaultHeight);
        const auto third = row1.getWidth() / 3;
        tapEvery->setBounds (row1.removeFromLeft (third));
        tempo->setBounds (row1.removeFromLeft (third));
        countIn->setBounds (row1);

        auto toggles = area.removeFromTop (28);
        fileTempo->setBounds (toggles.removeFromLeft (110).withX (tempo->getX() + (tempo->getWidth() - 110) / 2));
        loop->setBounds (toggles.withX (countIn->getX() + (countIn->getWidth() - 80) / 2).withWidth (80));
        area.removeFromTop (8);

        auto row2 = area.removeFromTop (Knob::kDefaultHeight);
        key1->setBounds (row2.removeFromLeft (third));
        key2->setBounds (row2.removeFromLeft (third));
        tapCC->setBounds (row2);
    }
}

void RKConductEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, songArea(), "Song");
    drawSection (g, conductArea(), "Conduct");

    // The song.
    auto info = songArea().reduced (8, 0).withTrimmedTop (kTitleHeight + 32).removeFromTop (30);
    g.setFont (rkfx::Fonts::bold (14.0f));
    if (note.isNotEmpty())
    {
        g.setColour (rkfx::Palette::warning);
        g.drawText (note, info, juce::Justification::centredLeft, true);
    }
    else if (conductor.getSongName().isEmpty())
    {
        g.setColour (rkfx::Palette::textDim);
        g.drawText ("No MIDI file loaded", info, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (rkfx::Palette::text);
        g.drawText (conductor.getSongName(), info, juce::Justification::centredLeft, true);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::regular (12.0f));
        g.drawText (juce::String (conductor.getSongLength() / 4.0, 1) + " bars", info, juce::Justification::centredRight);
    }

    // Where it is, at what tempo.
    const auto& engine = conductor.getEngine();
    auto status = statusArea();
    juce::String text;
    if (conductor.getSongName().isEmpty())
        text = "Load a MIDI file to conduct";
    else if (! engine.isPlaying())
        text = engine.getTapsToStart() > 1 ? "Tap " + juce::String (engine.getTapsToStart()) + " times to start" : "Tap to start";
    else
    {
        const auto pos = engine.getPosition();
        text = "Bar " + juce::String ((int) (pos / 4.0) + 1) + ", beat " + juce::String ((int) std::fmod (pos, 4.0) + 1)
             + (engine.isWaiting() ? "  (waiting for the tap)" : "");
    }
    g.setColour (engine.isPlaying() && ! engine.isWaiting() ? rkfx::Palette::text : rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (16.0f));
    g.drawText (text, status.removeFromTop (24), juce::Justification::centredLeft);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (13.0f));
    g.drawText (juce::String (engine.getTempo(), 1) + " BPM", status, juce::Justification::centredLeft);

    // Progress through the song.
    if (conductor.getSongLength() > 0.0)
    {
        auto bar = status.removeFromBottom (6).withTrimmedLeft (90).toFloat();
        g.setColour (rkfx::Palette::knobTrack);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (accent());
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, engine.getPosition() / conductor.getSongLength())), 2.0f);
    }
}

void RKConductEditor::refresh()
{
    if (conductor.getSongVersion() != shownVersion)
    {
        shownVersion = conductor.getSongVersion();
        rebuildRows();
    }
    repaint (statusArea());
}
