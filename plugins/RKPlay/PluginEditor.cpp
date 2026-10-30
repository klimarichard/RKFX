#include "PluginEditor.h"

namespace
{
    constexpr int kNowPlayingHeight = 214;
    constexpr int kTransportHeight = 56;
    constexpr int kRowHeight = 30;
    constexpr int kSettingsWidth = 260;
    constexpr int kButtonsHeight = 32;
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
    setUp (target, -23.0, -14.0, player.getShow().level.targetLufs, [this] (double v) { player.setTarget (v); });
    target.setTextValueSuffix (" LUFS");
    target.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 22);
    crossfade.setTooltip ("How long songs overlap; 0 plays them back to back, with no gap");
    fadeOutTime.setTooltip ("How long the Fade out button takes");
    target.setTooltip ("Every song plays this loud (integrated loudness); -18 LUFS is the archive's standard");

    smart.setTooltip ("Skip the silence at songs' starts and ends, and bring the next song in where a long fade-out has gone quiet");
    match.setTooltip ("Play every song at the same loudness, by gain alone (measured once per song)");
    safety.setTooltip ("A limiter on the output at -0.3 dBFS, for overlaps the analysis can't foresee");
    smart.onClick = [this] { player.setSmartCrossfade (smart.getToggleState()); };
    match.onClick = [this] { player.setLevelMatch (match.getToggleState()); };
    safety.onClick = [this] { player.setSafetyLimiter (safety.getToggleState()); };
    for (auto* t : { &smart, &match, &safety })
        addToCanvas (*t);

    lift.addItem ("Off", 1);
    lift.addItem ("Up to 3 dB", 2);
    lift.addItem ("Up to 6 dB", 3);
    lift.setTooltip ("Lift songs too dynamic to reach the target by gain: their peaks are limited by at most this much");
    lift.onChange = [this] { player.setLift (lift.getSelectedId() == 3 ? 6.0 : lift.getSelectedId() == 2 ? 3.0 : 0.0); };
    addToCanvas (lift);

    level = std::make_unique<rkfx::Knob> (*player.findParam ("level"), "Level");
    addToCanvas (*level);

    note.setJustificationType (juce::Justification::centredLeft);
    note.setColour (juce::Label::textColourId, rkfx::Palette::warning);
    note.setFont (rkfx::Fonts::regular (14.0f));
    addToCanvas (note);
    status.setJustificationType (juce::Justification::centredLeft);
    status.setColour (juce::Label::textColourId, rkfx::Palette::textDim);
    status.setFont (rkfx::Fonts::regular (13.0f));
    addToCanvas (status);

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

    // Right: the playlist's settings, then loudness.
    auto settings = r.removeFromRight (kSettingsWidth);
    r.removeFromRight (kGap);
    playlistArea = settings.removeFromTop (172);
    settings.removeFromTop (kGap);
    loudnessArea = settings;

    auto s = playlistArea.reduced (12, 8).withTrimmedTop (22);
    s.removeFromTop (18);
    crossfade.setBounds (s.removeFromTop (26));
    s.removeFromTop (22);
    fadeOutTime.setBounds (s.removeFromTop (26));
    s.removeFromTop (8);
    smart.setBounds (s.removeFromTop (26));

    s = loudnessArea.reduced (12, 8).withTrimmedTop (22);
    match.setBounds (s.removeFromTop (26));
    s.removeFromTop (22);
    target.setBounds (s.removeFromTop (26));
    s.removeFromTop (22);
    lift.setBounds (s.removeFromTop (26).withTrimmedRight (60));
    s.removeFromTop (8);
    safety.setBounds (s.removeFromTop (26));

    // Left: the list, its buttons under it.
    auto listButtons = r.removeFromBottom (kButtonsHeight);
    r.removeFromBottom (8);
    listArea = r;
    list.setBounds (r);
    addFilesButton.setBounds (listButtons.removeFromLeft (130));
    listButtons.removeFromLeft (8);
    addFolderButton.setBounds (listButtons.removeFromLeft (130));
    clearButton.setBounds (listButtons.removeFromRight (90));

    auto g = globalArea();
    level->setBounds (g.removeFromLeft (90));
    g.removeFromLeft (16);
    auto texts = g.withTrimmedRight (140).withSizeKeepingCentre (g.getWidth() - 140, 52);
    note.setBounds (texts.removeFromTop (26));
    status.setBounds (texts);
}

void RKPlayEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, playlistArea, "Playlist");
    drawSection (g, loudnessArea, "Loudness");
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (13.0f));
    const auto label = [&] (juce::Component& c, const juce::String& text)
    {
        g.drawText (text, c.getBounds().withY (c.getY() - 18).withHeight (16), juce::Justification::centredLeft);
    };
    label (crossfade, "Crossfade");
    label (fadeOutTime, "Fade out");
    label (target, "Target");
    label (lift, "Lift quiet songs");

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
    return getLocalBounds().reduced (20, 0).withTrimmedTop (104).withHeight (52);
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

    // The overview (the song's waveform once measured, else a bar), played part in the accent.
    const auto bar = barArea().toFloat();
    g.setColour (rkfx::Palette::background);
    g.fillRoundedRectangle (bar, 4.0f);
    const auto played = shown && length > 0.0 ? (float) juce::jlimit (0.0, 1.0, position / length) : 0.0f;
    const auto playedColour = owner.accent().withAlpha (state == RKPlayProcessor::State::paused ? 0.5f : 1.0f);
    const auto* overview = has && songs[(size_t) index].analysis.valid ? &songs[(size_t) index].analysis.overview : nullptr;
    if (overview != nullptr && ! overview->empty())
    {
        const auto points = (int) overview->size();
        const auto columns = (int) bar.getWidth() / 2;
        const auto mid = bar.getCentreY();
        for (int c = 0; c < columns; ++c)
        {
            // The loudest point under this column.
            const auto from = c * points / columns, to = juce::jmax (from + 1, (c + 1) * points / columns);
            int peak = 0;
            for (int i = from; i < to; ++i)
                peak = juce::jmax (peak, (int) (*overview)[(size_t) i]);
            const auto x = bar.getX() + (float) c * 2.0f;
            const auto h = juce::jmax (1.0f, (bar.getHeight() - 6.0f) * 0.5f * (float) peak / 255.0f);
            g.setColour ((float) c / (float) columns < played ? playedColour : rkfx::Palette::knobTrack);
            g.fillRect (x, mid - h, 1.5f, 2.0f * h);
        }
    }
    else if (played > 0.0f)
    {
        g.setColour (playedColour);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * played), 4.0f);
    }
    if (shown)
    {
        g.setColour (rkfx::Palette::text);
        g.fillRect (bar.getX() + bar.getWidth() * played - 1.0f, bar.getY(), 2.0f, bar.getHeight());
    }

    // What's next.
    const auto next = p.getNextIndex();
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (17.0f));
    g.drawFittedText ("Next:  " + (next >= 0 && next < (int) songs.size() ? songs[(size_t) next].displayName() : juce::String ("-")),
                      getLocalBounds().reduced (20, 0).withTrimmedTop (168).withHeight (30), juce::Justification::centredLeft, 1);
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

    const auto right = r.removeFromRight (100);
    const auto levelArea = r.removeFromRight (110);
    g.setFont (rkfx::Fonts::regular (15.0f));
    g.setColour (colour);
    g.drawFittedText (song.displayName(), r, juce::Justification::centredLeft, 1);

    // Its level: the gain loudness matching gives it, a lift (limited) in the warning colour.
    g.setFont (rkfx::Fonts::regular (13.0f));
    if (song.isPlayable())
    {
        const auto plan = player.levelOf (song);
        const auto db = [] (double v) { return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB"; };
        if (! song.analysis.valid && player.getShow().level.match)
        {
            g.setColour (rkfx::Palette::textDim.withAlpha (0.6f));
            g.drawText ("measuring", levelArea, juce::Justification::centredRight);
        }
        else if (plan.limitDb > 0.0)
        {
            g.setColour (rkfx::Palette::warning);
            g.drawText ("lift " + db (plan.limitDb), levelArea, juce::Justification::centredRight);
        }
        else if (! juce::exactlyEqual (plan.gainDb, 0.0))
        {
            g.setColour (rkfx::Palette::textDim);
            g.drawText (db (plan.gainDb), levelArea, juce::Justification::centredRight);
        }
    }

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
    juce::PopupMenu gainMenu;
    const auto gainNow = songs[(size_t) row].gainDb;
    for (int db = 6; db >= -12; --db)
        gainMenu.addItem (100 + db + 12, (db > 0 ? "+" : "") + juce::String (db) + " dB", true, std::abs (gainNow - db) < 0.01);
    menu.addSubMenu ("Song gain", gainMenu);
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
            default:
                if (result >= 100)
                    p.setSongGain (row, result - 100 - 12);
                else
                    return;
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
    const auto& show = player.getShow();
    target.setValue (show.level.targetLufs, juce::dontSendNotification);
    smart.setToggleState (show.smartCrossfade, juce::dontSendNotification);
    match.setToggleState (show.level.match, juce::dontSendNotification);
    safety.setToggleState (show.safetyLimiter, juce::dontSendNotification);
    lift.setSelectedId (show.level.liftDb > 4.5 ? 3 : show.level.liftDb > 0.5 ? 2 : 1, juce::dontSendNotification);
    target.setEnabled (show.level.match);
    lift.setEnabled (show.level.match);
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

    // The status line: songs still being measured; the safety limiter working (held 2 s).
    const auto reduction = player.takeLimiterReductionDb();
    if (reduction < -0.1f)
    {
        limiterDb = juce::jmin (now - limiterTime < 2.0 ? limiterDb : 0.0f, reduction);
        limiterTime = now;
    }
    juce::StringArray parts;
    if (const auto pending = player.analysisPending(); pending > 0)
        parts.add ("Measuring " + juce::String (pending) + (pending == 1 ? " song" : " songs") + "...");
    if (now - limiterTime < 2.0)
        parts.add ("Safety limiter " + juce::String (limiterDb, 1) + " dB");
    status.setText (parts.joinIntoString ("    "), juce::dontSendNotification);
}

juce::String RKPlayEditor::timeText (double seconds)
{
    const auto total = (int) std::floor (juce::jmax (0.0, seconds));
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
