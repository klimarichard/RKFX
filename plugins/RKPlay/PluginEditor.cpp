#include "PluginEditor.h"

namespace
{
    constexpr int kNowPlayingHeight = 186;
    constexpr int kTransportHeight = 56;
    constexpr int kRowHeight = 30;
    constexpr int kSettingsWidth = 250;
    constexpr int kGap = 10;
}

RKPlayEditor::RKPlayEditor (RKPlayProcessor& p)
    : rkfx::EditorBase (p, kWidth, kHeight), player (p)
{
    addToCanvas (nowPlaying);

    prevButton.setTooltip ("Previous song (or this one from the start, after 5 s)");
    playButton.setTooltip ("Play / Pause");
    nextButton.setTooltip ("Next song, crossfading");
    fadeButton.setTooltip ("Fade out and stop; Play goes on with the next song");
    stopButton.setTooltip ("Stop (a short fade); right-click: Panic");
    prevButton.onClick = [this] { player.previous(); refresh(); };
    playButton.onClick = [this] { player.togglePlay(); refresh(); };
    nextButton.onClick = [this] { player.next(); refresh(); };
    fadeButton.onClick = [this] { player.fadeOut(); refresh(); };
    stopButton.onClick = [this] { player.stop(); refresh(); };
    for (auto* b : { &prevButton, &playButton, &nextButton, &fadeButton, &stopButton })
        addToCanvas (*b);

    list.setRowHeight (kRowHeight);
    list.setMultipleSelectionEnabled (false);
    list.setColour (juce::ListBox::backgroundColourId, rkfx::Palette::background);
    list.setColour (juce::ListBox::outlineColourId, rkfx::Palette::outline);
    list.setOutlineThickness (1);
    addToCanvas (list);

    addFilesButton.onClick = [this] { chooseFiles (false); };
    addFolderButton.onClick = [this] { chooseFiles (true); };
    clearButton.onClick = [this] { player.clearSongs(); refresh(); };
    clearButton.setTooltip ("Remove every song from the playlist (stops the music)");
    for (auto* b : { &addFilesButton, &addFolderButton, &clearButton })
        addToCanvas (*b);

    const auto setUp = [this] (juce::Slider& s, double min, double max, double value, const std::function<void (double)>& set)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
        s.setRange (min, max, 0.5);
        s.setTextValueSuffix (" s");
        s.setValue (value, juce::dontSendNotification);
        s.onValueChange = [&s, set] { set (s.getValue()); };
        addToCanvas (s);
    };
    setUp (crossfade, 0.0, PlayShow::kMaxCrossfade, player.getShow().crossfadeSeconds, [this] (double v) { player.setCrossfade (v); });
    setUp (fadeOutTime, 0.5, 30.0, player.getShow().fadeOutSeconds, [this] (double v) { player.setFadeOut (v); });
    crossfade.setTooltip ("How long songs overlap; 0 plays them back to back, with no gap");
    fadeOutTime.setTooltip ("How long the Fade out button takes");

    level = std::make_unique<rkfx::Knob> (*player.findParam ("level"), "Level");
    addToCanvas (*level);

    note.setJustificationType (juce::Justification::centredLeft);
    note.setColour (juce::Label::textColourId, rkfx::Palette::warning);
    note.setFont (rkfx::Fonts::regular (14.0f));
    addToCanvas (note);

    addMouseListener (this, true);
    finishSetup();
    refresh();
    timer.startTimerHz (15);
}

RKPlayEditor::~RKPlayEditor()
{
    timer.stopTimer();
}

void RKPlayEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && e.originalComponent == &stopButton)
    {
        player.panic();
        refresh();
    }
}

//==============================================================================
void RKPlayEditor::layoutContent()
{
    auto r = contentArea();
    nowPlaying.setBounds (r.removeFromTop (kNowPlayingHeight));
    r.removeFromTop (kGap);

    auto transport = r.removeFromTop (kTransportHeight);
    const int widths[] = { 80, 180, 80, 140, 100 };
    juce::TextButton* buttons[] = { &prevButton, &playButton, &nextButton, &fadeButton, &stopButton };
    for (int i = 0; i < 5; ++i)
    {
        buttons[i]->setBounds (transport.removeFromLeft (widths[i]));
        transport.removeFromLeft (kGap);
    }
    r.removeFromTop (kGap);

    settingsArea = r.removeFromRight (kSettingsWidth);
    r.removeFromRight (kGap);
    listArea = r;
    list.setBounds (r);

    auto s = settingsArea.reduced (12, 10).withTrimmedTop (22);
    s.removeFromTop (18);
    crossfade.setBounds (s.removeFromTop (28));
    s.removeFromTop (26);
    fadeOutTime.setBounds (s.removeFromTop (28));
    s.removeFromTop (20);
    addFilesButton.setBounds (s.removeFromTop (30));
    s.removeFromTop (8);
    addFolderButton.setBounds (s.removeFromTop (30));
    s.removeFromTop (8);
    clearButton.setBounds (s.removeFromTop (30));

    auto g = globalArea();
    level->setBounds (g.removeFromLeft (90));
    g.removeFromLeft (16);
    note.setBounds (g.withTrimmedRight (140).withSizeKeepingCentre (g.getWidth() - 140, 40));
}

void RKPlayEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, settingsArea, "Playlist");
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (13.0f));
    const auto label = [&] (juce::Component& c, const juce::String& text)
    {
        g.drawText (text, c.getBounds().withY (c.getY() - 18).withHeight (16), juce::Justification::centredLeft);
    };
    label (crossfade, "Crossfade");
    label (fadeOutTime, "Fade out");

    // An empty playlist says how to fill it; a drag over the window lights it up.
    if (player.getShow().songs.empty())
    {
        g.setFont (rkfx::Fonts::regular (16.0f));
        g.drawText ("Drop audio files or folders here, or use Add files", listArea, juce::Justification::centred);
    }
    if (dropHighlight)
    {
        g.setColour (accent());
        g.drawRoundedRectangle (listArea.toFloat().reduced (1.0f), rkfx::Style::kPanelRadius, 2.0f);
    }
}

//==============================================================================
juce::Rectangle<int> RKPlayEditor::NowPlaying::barArea() const
{
    return getLocalBounds().reduced (20, 0).withTrimmedTop (114).withHeight (16);
}

void RKPlayEditor::NowPlaying::paint (juce::Graphics& g)
{
    auto& p = owner.player;
    const auto& songs = p.getShow().songs;
    auto r = getLocalBounds();

    g.setColour (owner.panelColour());
    g.fillRoundedRectangle (r.toFloat(), rkfx::Style::kPanelRadius);

    auto inner = r.reduced (20, 14);
    const auto index = p.getCurrentIndex();
    const auto has = index >= 0 && index < (int) songs.size();
    const auto state = p.getState();

    // Times (right): elapsed and remaining, large.
    auto times = inner.removeFromRight (300).removeFromTop (86);
    const auto position = p.getPositionSeconds();
    const auto length = p.getLengthSeconds();
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (15.0f));
    g.drawText ("elapsed", times.withWidth (130).removeFromTop (18), juce::Justification::centredRight);
    g.drawText ("remaining", times.withTrimmedLeft (150).removeFromTop (18), juce::Justification::centredRight);
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (40.0f));
    const auto shown = state != RKPlayProcessor::State::stopped;
    g.drawText (shown ? timeText (position) : "--:--", times.withWidth (130).withTrimmedTop (20), juce::Justification::centredRight);
    g.setColour (shown ? owner.accent() : rkfx::Palette::textDim);
    g.drawText (shown ? "-" + timeText (juce::jmax (0.0, length - position)) : timeText (length),
                times.withTrimmedLeft (150).withTrimmedTop (20), juce::Justification::centredRight);

    // Names (left), large.
    auto names = inner.removeFromTop (86);
    g.setColour (state == RKPlayProcessor::State::stopped ? rkfx::Palette::textDim : rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (34.0f));
    g.drawFittedText (has ? songs[(size_t) index].title : juce::String ("No songs"), names.removeFromTop (48),
                      juce::Justification::centredLeft, 1, 0.8f);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (20.0f));
    const auto status = state == RKPlayProcessor::State::paused ? "Paused" : state == RKPlayProcessor::State::stopped ? "Stopped" : "";
    g.drawFittedText ((has ? songs[(size_t) index].artist : juce::String()) + (juce::String (status).isNotEmpty() && has && songs[(size_t) index].artist.isNotEmpty() ? "   ·   " : "") + status,
                      names, juce::Justification::centredLeft, 1);

    // The progress bar.
    const auto bar = barArea().toFloat();
    g.setColour (rkfx::Palette::background);
    g.fillRoundedRectangle (bar, 4.0f);
    if (shown && length > 0.0)
    {
        g.setColour (owner.accent().withAlpha (state == RKPlayProcessor::State::paused ? 0.5f : 1.0f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, position / length)), 4.0f);
    }

    // What's next.
    const auto next = p.getNextIndex();
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (17.0f));
    g.drawFittedText ("Next:  " + (next >= 0 && next < (int) songs.size() ? songs[(size_t) next].displayName() : juce::String ("-")),
                      getLocalBounds().reduced (20, 0).withTrimmedTop (140).withHeight (30), juce::Justification::centredLeft, 1);
}

void RKPlayEditor::NowPlaying::mouseDown (const juce::MouseEvent& e)
{
    const auto bar = barArea().expanded (0, 8);
    const auto length = owner.player.getLengthSeconds();
    if (bar.contains (e.getPosition()) && length > 0.0)
        owner.player.seek (length * juce::jlimit (0.0, 1.0, (double) (e.x - bar.getX()) / bar.getWidth()));
}

//==============================================================================
// The playlist

int RKPlayEditor::getNumRows()
{
    return (int) player.getShow().songs.size();
}

void RKPlayEditor::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto& songs = player.getShow().songs;
    if (row < 0 || row >= (int) songs.size())
        return;
    const auto& song = songs[(size_t) row];
    const auto current = row == player.getCurrentIndex();
    const auto playing = current && player.getState() != RKPlayProcessor::State::stopped;
    const auto next = row == player.getNextIndex() && player.getState() != RKPlayProcessor::State::stopped;

    if (selected)
        g.fillAll (rkfx::Palette::knobBody);
    if (playing)
    {
        g.setColour (accent().withAlpha (0.25f));
        g.fillRect (0, 0, width, height);
    }

    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (8, 0);
    const auto colour = ! song.isPlayable() ? juce::Colour (0xffe05050) : current ? rkfx::Palette::text : rkfx::Palette::text.withAlpha (0.85f);

    g.setFont (rkfx::Fonts::regular (13.0f));
    g.setColour (rkfx::Palette::textDim);
    g.drawText (playing ? juce::String::fromUTF8 ("\xe2\x96\xb6") : juce::String (row + 1), r.removeFromLeft (34), juce::Justification::centredLeft);

    const auto right = r.removeFromRight (150);
    g.setFont (rkfx::Fonts::regular (15.0f));
    g.setColour (colour);
    g.drawFittedText (song.displayName(), r, juce::Justification::centredLeft, 1);

    g.setFont (rkfx::Fonts::regular (13.0f));
    g.setColour (song.isPlayable() ? rkfx::Palette::textDim : colour);
    const auto status = song.status == PlaySong::Status::missing ? juce::String ("missing")
                      : song.status == PlaySong::Status::unreadable ? juce::String ("unreadable")
                      : next ? juce::String ("next   ") + timeText (song.lengthSeconds) : timeText (song.lengthSeconds);
    g.drawText (status, right, juce::Justification::centredRight);
}

void RKPlayEditor::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    player.playSong (row);
    refresh();
}

void RKPlayEditor::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showSongMenu (row);
}

void RKPlayEditor::deleteKeyPressed (int row)
{
    player.removeSong (row);
    refresh();
}

void RKPlayEditor::showSongMenu (int row)
{
    const auto& songs = player.getShow().songs;
    if (row < 0 || row >= (int) songs.size())
        return;
    const auto playingThis = row == player.getCurrentIndex() && player.getState() != RKPlayProcessor::State::stopped;

    juce::PopupMenu menu;
    menu.addItem (1, "Play now", songs[(size_t) row].isPlayable());
    menu.addSeparator();
    menu.addItem (2, "Move up", row > 0);
    menu.addItem (3, "Move down", row < (int) songs.size() - 1);
    menu.addItem (4, "Remove", ! playingThis);
    menu.addSeparator();
    menu.addItem (5, "Show in Finder", songs[(size_t) row].file.exists());

    juce::Component::SafePointer<RKPlayEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, row] (int result)
    {
        if (safe == nullptr)
            return;
        auto& p = safe->player;
        switch (result)
        {
            case 1: p.playSong (row); break;
            case 2: p.moveSong (row, row - 1); safe->list.selectRow (row - 1); break;
            case 3: p.moveSong (row, row + 1); safe->list.selectRow (row + 1); break;
            case 4: p.removeSong (row); break;
            case 5: p.getShow().songs[(size_t) row].file.revealToUser(); break;
            default: return;
        }
        safe->refresh();
    });
}

//==============================================================================
void RKPlayEditor::filesDropped (const juce::StringArray& paths, int x, int y)
{
    dropHighlight = false;
    juce::Array<juce::File> files;
    for (const auto& path : paths)
        files.add (juce::File (path));

    // Onto a row: before it; anywhere else: at the end.
    const auto inList = list.getLocalPoint (this, juce::Point<int> (x, y));
    auto row = list.getLocalBounds().contains (inList) ? list.getRowContainingPosition (inList.x, inList.y) : -1;
    player.addFiles (files, row);
    refresh();
}

void RKPlayEditor::chooseFiles (bool folders)
{
    chooser = std::make_unique<juce::FileChooser> (folders ? "Add a folder of songs" : "Add songs",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   folders ? juce::String() : player.getFormats().getWildcardForAllFormats());
    const auto flags = juce::FileBrowserComponent::openMode
                     | (folders ? juce::FileBrowserComponent::canSelectDirectories
                                : juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems);
    juce::Component::SafePointer<RKPlayEditor> safe (this);
    chooser->launchAsync (flags, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResults().isEmpty())
            return;
        safe->player.addFiles (fc.getResults());
        safe->refresh();
    });
}

//==============================================================================
void RKPlayEditor::refresh()
{
    lastShowVersion = player.getShowVersion();
    list.updateContent();
    list.repaint();
    const auto state = player.getState();
    playButton.setButtonText (state == RKPlayProcessor::State::playing ? "Pause" : "Play");
    playButton.setToggleState (state == RKPlayProcessor::State::playing, juce::dontSendNotification);
    crossfade.setValue (player.getShow().crossfadeSeconds, juce::dontSendNotification);
    fadeOutTime.setValue (player.getShow().fadeOutSeconds, juce::dontSendNotification);
    nowPlaying.repaint();
    repaint();
}

void RKPlayEditor::tick()
{
    if (player.getShowVersion() != lastShowVersion)
        refresh();
    else
        nowPlaying.repaint();

    // A note shows for 8 seconds.
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (player.getNoteVersion() != lastNoteVersion)
    {
        lastNoteVersion = player.getNoteVersion();
        note.setText (player.getNote(), juce::dontSendNotification);
        noteTime = now;
    }
    else if (note.getText().isNotEmpty() && now - noteTime > 8.0)
    {
        note.setText ({}, juce::dontSendNotification);
    }
}

juce::String RKPlayEditor::timeText (double seconds)
{
    const auto total = (int) std::floor (juce::jmax (0.0, seconds));
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
