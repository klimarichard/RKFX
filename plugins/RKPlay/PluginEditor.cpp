#include "PluginEditor.h"

namespace
{
    constexpr int kNowPlayingHeight = 214;
    constexpr int kTransportHeight = 56;
    constexpr int kRowHeight = 30;
    constexpr int kSideWidth = 270;
    constexpr int kTabsHeight = 30;
    constexpr int kBarHeight = 32;
    constexpr int kInfoHeight = 34;
    constexpr int kButtonsHeight = 32;
    constexpr int kTabWidth = 130;
    constexpr int kGap = 10;
    const juce::Colour kProblem { 0xffe05050 };

    void setUpSlider (juce::Slider& s, double min, double max, double step, const juce::String& suffix, int textWidth)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, textWidth, 22);
        s.setRange (min, max, step);
        s.setTextValueSuffix (suffix);
    }

    juce::String dbText (double v)
    {
        return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB";
    }
}

RKPlayEditor::RKPlayEditor (RKPlayProcessor& p)
    : rkfx::EditorBase (p, kWidth, kHeight), player (p)
{
    viewed = player.getShow().active;
    addToCanvas (nowPlaying);

    // Transport
    prevButton.setTooltip ("Previous song (or this one from the start, after 5 s)");
    playButton.setTooltip ("Play / Pause");
    nextButton.setTooltip ("Next song, crossfading");
    fadeButton.setTooltip ("Fade out and stop; Play goes on with the next song");
    stopButton.setTooltip ("Stop (a short fade); right-click: Panic");
    settingsButton.setTooltip ("Fade out, smart crossfade, loudness, the Music folder");
    prevButton.onClick = [this] { player.previous(); refresh(); };
    playButton.onClick = [this] { player.togglePlay(); refresh(); };
    nextButton.onClick = [this] { player.next(); refresh(); };
    fadeButton.onClick = [this] { player.fadeOut(); refresh(); };
    stopButton.onClick = [this] { player.stop(); refresh(); };
    settingsButton.onClick = [this] { showSettings (! settings.isVisible()); };
    for (auto* b : { &prevButton, &playButton, &nextButton, &fadeButton, &stopButton, &settingsButton })
        addToCanvas (*b);

    // The viewed playlist: tabs, its settings, its songs
    addToCanvas (tabs);
    inOrderButton.onClick = [this] { player.setOrder (viewed, PlayPlaylist::Order::inOrder); };
    shuffleButton.onClick = [this] { player.setOrder (viewed, PlayPlaylist::Order::shuffle); };
    shuffleButton.setTooltip ("Every song once, in a random order, before any plays again");
    atEnd.setTooltip ("What happens after the last song");
    atEnd.onChange = [this]
    {
        const auto id = atEnd.getSelectedId();
        if (id == 1)      player.setAtEnd (viewed, PlayPlaylist::AtEnd::stop);
        else if (id == 2) player.setAtEnd (viewed, PlayPlaylist::AtEnd::repeat);
        else if (id >= 10) player.setAtEnd (viewed, PlayPlaylist::AtEnd::goTo, id - 10);
    };
    stopAfterEach.setTooltip ("Wait for Play after every song (a ceremony, songs started by hand)");
    stopAfterEach.onClick = [this] { player.setStopAfterEach (viewed, stopAfterEach.getToggleState()); };
    setUpSlider (crossfade, 0.0, PlayShow::kMaxCrossfade, 0.5, " s", 50);
    crossfade.setTooltip ("How long songs overlap; 0 plays them back to back, with no gap");
    crossfade.onValueChange = [this] { player.setPlaylistCrossfade (viewed, crossfade.getValue()); };
    setUpSlider (listGain, -12.0, 12.0, 0.5, " dB", 60);
    listGain.setTooltip ("The whole list louder or quieter, on top of loudness matching (a quieter Dinner list)");
    listGain.onValueChange = [this] { player.setPlaylistGain (viewed, listGain.getValue()); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &inOrderButton, &shuffleButton, &atEnd, &stopAfterEach, &crossfade, &listGain })
        addToCanvas (*c);

    list.setRowHeight (kRowHeight);
    list.setMultipleSelectionEnabled (false);
    list.setColour (juce::ListBox::backgroundColourId, rkfx::Palette::background);
    list.setColour (juce::ListBox::outlineColourId, rkfx::Palette::outline);
    list.setOutlineThickness (1);
    addToCanvas (list);

    addFilesButton.onClick = [this] { chooseFiles (false); };
    addFolderButton.onClick = [this] { chooseFiles (true); };
    clearButton.onClick = [this] { player.clearSongs (viewed); refresh(); };
    clearButton.setTooltip ("Remove every song from this playlist");
    for (auto* b : { &addFilesButton, &addFolderButton, &clearButton })
        addToCanvas (*b);

    // The queue and End at
    queueList.setRowHeight (26);
    queueList.setColour (juce::ListBox::backgroundColourId, rkfx::Palette::background);
    queueList.setColour (juce::ListBox::outlineColourId, rkfx::Palette::outline);
    queueList.setOutlineThickness (1);
    addToCanvas (queueList);
    clearQueueButton.onClick = [this] { player.clearQueue(); };
    addToCanvas (clearQueueButton);

    endAtOn.setTooltip ("Stop the music at a time of day (music until the band comes back)");
    endAtTime.setJustification (juce::Justification::centred);
    endAtTime.setInputRestrictions (5, "0123456789:");
    endAtTime.setFont (rkfx::Fonts::bold (16.0f));
    endAtMode.addItem ("Fade out exactly then", 1);
    endAtMode.addItem ("After the song playing then", 2);
    endAtOn.onClick = [this] { applyEndAt(); };
    endAtTime.onReturnKey = [this] { applyEndAt(); };
    endAtTime.onFocusLost = [this] { applyEndAt(); };
    endAtMode.onChange = [this] { applyEndAt(); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &endAtOn, &endAtTime, &endAtMode })
        addToCanvas (*c);

    level = std::make_unique<rkfx::Knob> (*player.findParam ("level"), "Level");
    addToCanvas (*level);

    note.setJustificationType (juce::Justification::centredLeft);
    note.setColour (juce::Label::textColourId, rkfx::Palette::warning);
    note.setFont (rkfx::Fonts::regular (14.0f));
    status.setJustificationType (juce::Justification::centredLeft);
    status.setColour (juce::Label::textColourId, rkfx::Palette::textDim);
    status.setFont (rkfx::Fonts::regular (13.0f));
    addToCanvas (note);
    addToCanvas (status);

    addToCanvas (settings);
    settings.setVisible (false);

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
    settingsButton.setBounds (transport.removeFromRight (110));
    const int widths[] = { 80, 180, 80, 140, 100 };
    juce::TextButton* buttons[] = { &prevButton, &playButton, &nextButton, &fadeButton, &stopButton };
    for (int i = 0; i < 5; ++i)
    {
        buttons[i]->setBounds (transport.removeFromLeft (widths[i]));
        transport.removeFromLeft (kGap);
    }
    r.removeFromTop (kGap);
    const auto lower = r;

    // Right: the queue, then End at.
    auto side = r.removeFromRight (kSideWidth);
    r.removeFromRight (kGap);
    endAtArea = side.removeFromBottom (128);
    side.removeFromBottom (kGap);
    queueArea = side;
    {
        auto q = queueArea.reduced (10, 8).withTrimmedTop (22);
        clearQueueButton.setBounds (q.removeFromBottom (28));
        q.removeFromBottom (6);
        queueList.setBounds (q);

        auto e = endAtArea.reduced (10, 8).withTrimmedTop (22);
        auto row = e.removeFromTop (30);
        endAtOn.setBounds (row.removeFromLeft (110));
        endAtTime.setBounds (row.removeFromLeft (90).reduced (0, 2));
        e.removeFromTop (8);
        endAtMode.setBounds (e.removeFromTop (28));
    }

    // Left: tabs, the playlist's settings, its songs, its buttons.
    tabs.setBounds (r.removeFromTop (kTabsHeight));
    auto bar = r.removeFromTop (kBarHeight).withTrimmedTop (4);
    inOrderButton.setBounds (bar.removeFromLeft (80));
    shuffleButton.setBounds (bar.removeFromLeft (80));
    bar.removeFromLeft (12);
    atEnd.setBounds (bar.removeFromLeft (170));
    bar.removeFromLeft (12);
    stopAfterEach.setBounds (bar.removeFromLeft (140));
    // The info line: the list's size and link on the left, its crossfade and gain on the right.
    infoArea = r.removeFromTop (kInfoHeight);
    auto sliders = infoArea.reduced (0, 4);
    listGain.setBounds (sliders.removeFromRight (150));
    sliders.removeFromRight (40);   // "Gain"
    crossfade.setBounds (sliders.removeFromRight (190));
    sliders.removeFromRight (70);   // "Crossfade"
    infoArea = sliders;
    r.removeFromTop (4);

    auto listButtons = r.removeFromBottom (kButtonsHeight);
    r.removeFromBottom (8);
    listArea = r;
    list.setBounds (r);
    addFilesButton.setBounds (listButtons.removeFromLeft (130));
    listButtons.removeFromLeft (8);
    addFolderButton.setBounds (listButtons.removeFromLeft (130));
    clearButton.setBounds (listButtons.removeFromRight (90));

    settings.setBounds (lower);

    auto g = globalArea();
    level->setBounds (g.removeFromLeft (90));
    g.removeFromLeft (16);
    auto texts = g.withTrimmedRight (140).withSizeKeepingCentre (g.getWidth() - 140, 52);
    note.setBounds (texts.removeFromTop (26));
    status.setBounds (texts);
}

void RKPlayEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, queueArea, "Queue");
    drawSection (g, endAtArea, "End at");

    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (13.0f));
    g.drawText ("Crossfade", crossfade.getBounds().withX (crossfade.getX() - 70).withWidth (66), juce::Justification::centredRight);
    g.drawText ("Gain", listGain.getBounds().withX (listGain.getX() - 40).withWidth (36), juce::Justification::centredRight);
    if (player.getQueue().empty())
        g.drawFittedText ("Requests: right-click a song,\nPlay next", queueList.getBounds().reduced (10), juce::Justification::centred, 2);

    // The playlist's line: its size, its link; End at's countdown.
    const auto& p = viewedList();
    double total = 0.0;
    for (const auto& s : p.songs)
        total += s.lengthSeconds;
    juce::String info = juce::String ((int) p.songs.size()) + ((int) p.songs.size() == 1 ? " song, " : " songs, ") + timeText (total);
    if (p.isLinked())
        info << "   ·   linked to " << (p.link == PlayPlaylist::Link::folder ? "the folder " : "") << p.linkFile.getFileName()
             << " (right-click the tab to rescan)";
    if (player.getPendingSwitch() >= 0 && player.getPendingSwitch() < (int) player.getShow().playlists.size())
        info << "   ·   then " << player.getShow().playlists[(size_t) player.getPendingSwitch()].name;
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (13.0f));
    g.drawFittedText (info, infoArea.withTrimmedLeft (4), juce::Justification::centredLeft, 1);

    if (const auto left = player.getEndAtRemaining(); left >= 0.0)
    {
        const auto at = juce::String (player.getShow().endAt.minutes / 60) + ":" + juce::String (player.getShow().endAt.minutes % 60).paddedLeft ('0', 2);
        g.setColour (accent());
        g.setFont (rkfx::Fonts::bold (16.0f));
        g.drawText ("End at " + at + "  ·  in " + timeText (left), stopButton.getBounds().withX (stopButton.getRight() + 20).withRight (settingsButton.getX() - 12),
                    juce::Justification::centredRight);
    }

    // An empty playlist says how to fill it; a drag over the window lights it up.
    if (p.songs.empty())
    {
        g.setFont (rkfx::Fonts::regular (16.0f));
        g.setColour (rkfx::Palette::textDim);
        g.drawText (p.isLinked() ? "The linked folder or file has no songs" : "Drop audio files, folders or .m3u8 playlists here, or use Add files",
                    listArea, juce::Justification::centred);
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
    const auto& item = p.getCurrent();
    auto r = getLocalBounds();

    g.setColour (owner.panelColour());
    g.fillRoundedRectangle (r.toFloat(), rkfx::Style::kPanelRadius);

    auto inner = r.reduced (20, 14);
    const auto state = p.getState();
    const auto shown = state != RKPlayProcessor::State::stopped && item.isValid();

    // Stopped: what Play starts.
    const auto& song = shown ? item.song : owner.nextItem.song;
    const auto has = shown || owner.nextItem.isValid();

    // Times (right): elapsed and remaining, large.
    auto times = inner.removeFromRight (300).removeFromTop (86);
    const auto position = p.getPositionSeconds();
    const auto length = shown ? item.song.lengthSeconds : song.lengthSeconds;
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (15.0f));
    g.drawText ("elapsed", times.withWidth (130).removeFromTop (18), juce::Justification::centredRight);
    g.drawText ("remaining", times.withTrimmedLeft (150).removeFromTop (18), juce::Justification::centredRight);
    g.setColour (rkfx::Palette::text);
    g.setFont (rkfx::Fonts::bold (40.0f));
    g.drawText (shown ? timeText (position) : "--:--", times.withWidth (130).withTrimmedTop (20), juce::Justification::centredRight);
    g.setColour (shown ? owner.accent() : rkfx::Palette::textDim);
    g.drawText (shown ? "-" + timeText (juce::jmax (0.0, length - position)) : timeText (length),
                times.withTrimmedLeft (150).withTrimmedTop (20), juce::Justification::centredRight);

    // Names (left), large.
    auto names = inner.removeFromTop (86);
    g.setColour (shown ? rkfx::Palette::text : rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::bold (34.0f));
    g.drawFittedText (has ? song.title : juce::String ("No songs"), names.removeFromTop (48), juce::Justification::centredLeft, 1, 0.8f);
    juce::StringArray line;
    if (has && song.artist.isNotEmpty())
        line.add (song.artist);
    if (state == RKPlayProcessor::State::paused)
        line.add ("Paused");
    else if (! shown)
        line.add ("Stopped");
    if (shown && item.queued)
        line.add ("request");
    else if (shown && item.playlist >= 0 && item.playlist < (int) p.getShow().playlists.size())
        line.add (p.getShow().playlists[(size_t) item.playlist].name);
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (20.0f));
    g.drawFittedText (line.joinIntoString ("   ·   "), names, juce::Justification::centredLeft, 1);

    // The overview (the song's waveform once measured, else a bar), played part in the accent.
    const auto bar = barArea().toFloat();
    g.setColour (rkfx::Palette::background);
    g.fillRoundedRectangle (bar, 4.0f);
    const auto played = shown && length > 0.0 ? (float) juce::jlimit (0.0, 1.0, position / length) : 0.0f;
    const auto playedColour = owner.accent().withAlpha (state == RKPlayProcessor::State::paused ? 0.5f : 1.0f);
    const auto* overview = has && song.analysis.valid ? &song.analysis.overview : nullptr;
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
    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (17.0f));
    juce::String next ("-");
    if (shown && owner.nextItem.isValid())
        next = owner.nextItem.song.displayName() + (owner.nextItem.queued ? "  (queued)" : "");
    else if (shown && state != RKPlayProcessor::State::stopped && p.getShow().current().stopAfterEach)
        next = "stops after this song";
    g.drawFittedText ("Next:  " + next, getLocalBounds().reduced (20, 0).withTrimmedTop (168).withHeight (30), juce::Justification::centredLeft, 1);
}

void RKPlayEditor::NowPlaying::mouseDown (const juce::MouseEvent& e)
{
    const auto bar = barArea().expanded (0, 8);
    const auto length = owner.player.getCurrent().song.lengthSeconds;
    if (bar.contains (e.getPosition()) && length > 0.0)
        owner.player.seek (length * juce::jlimit (0.0, 1.0, (double) (e.x - bar.getX()) / bar.getWidth()));
}

//==============================================================================
// Tabs

juce::Rectangle<int> RKPlayEditor::Tabs::tabBounds (int index) const
{
    const auto count = (int) owner.player.getShow().playlists.size();
    const auto width = juce::jmin (kTabWidth, (getWidth() - 40) / juce::jmax (1, count));
    return { index * (width + 2), 0, index == count ? 36 : width, getHeight() };
}

int RKPlayEditor::Tabs::tabAt (juce::Point<int> p) const
{
    const auto count = (int) owner.player.getShow().playlists.size();
    for (int i = 0; i < count; ++i)
        if (tabBounds (i).contains (p))
            return i;
    return count < PlayShow::kMaxPlaylists && tabBounds (count).contains (p) ? -2 : -1;
}

void RKPlayEditor::Tabs::paint (juce::Graphics& g)
{
    const auto& show = owner.player.getShow();
    const auto playing = owner.player.getState() != RKPlayProcessor::State::stopped ? show.active : -1;
    for (int i = 0; i < (int) show.playlists.size(); ++i)
    {
        const auto b = tabBounds (i).toFloat();
        const auto selected = i == owner.viewed;
        g.setColour (selected ? owner.panelColour().brighter (0.15f) : owner.panelColour());
        g.fillRoundedRectangle (b.withTrimmedBottom (-4.0f), 5.0f);
        if (i == playing)
        {
            g.setColour (owner.accent());
            g.fillRect (b.getX() + 6.0f, b.getBottom() - 3.0f, b.getWidth() - 12.0f, 3.0f);
        }
        g.setColour (selected ? rkfx::Palette::text : rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (14.0f));
        const auto name = (i == playing ? juce::String::fromUTF8 ("\xe2\x96\xb6 ") : juce::String())
                        + (show.playlists[(size_t) i].isLinked() ? juce::String::fromUTF8 ("\xe2\x86\xbb ") : juce::String())
                        + show.playlists[(size_t) i].name;
        g.drawFittedText (name, b.toNearestInt().reduced (8, 0), juce::Justification::centred, 1, 0.8f);
    }
    if ((int) show.playlists.size() < PlayShow::kMaxPlaylists)
    {
        const auto b = tabBounds ((int) show.playlists.size()).toFloat();
        g.setColour (owner.panelColour());
        g.fillRoundedRectangle (b.reduced (2.0f, 3.0f), 5.0f);
        g.setColour (rkfx::Palette::textDim);
        g.setFont (rkfx::Fonts::bold (18.0f));
        g.drawText ("+", b, juce::Justification::centred);
    }
}

void RKPlayEditor::Tabs::mouseDown (const juce::MouseEvent& e)
{
    const auto i = tabAt (e.getPosition());
    if (i == -2)
    {
        if (const auto added = owner.player.addPlaylist(); added >= 0)
            owner.viewed = added;
        owner.refresh();
        return;
    }
    if (i < 0)
        return;
    owner.viewed = i;
    owner.refresh();
    if (e.mods.isPopupMenu())
        owner.showTabMenu (i);
}

void RKPlayEditor::Tabs::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (const auto i = tabAt (e.getPosition()); i >= 0)
        owner.renamePlaylist (i);
}

void RKPlayEditor::showTabMenu (int playlist)
{
    const auto& show = player.getShow();
    const auto& p = show.playlists[(size_t) playlist];
    const auto playing = player.getState() != RKPlayProcessor::State::stopped;
    const auto isActive = playlist == show.active;

    juce::PopupMenu menu;
    menu.addItem (1, playing ? "Play this playlist now (crossfade)" : "Play this playlist", ! (isActive && playing) && ! p.songs.empty());
    menu.addItem (2, "Play this playlist after the current song", playing && ! isActive && ! p.songs.empty());
    menu.addSeparator();
    menu.addItem (3, "Rename...");
    menu.addItem (4, "Link to a folder...");
    menu.addItem (5, "Link to an .m3u8 playlist...");
    menu.addItem (6, "Rescan", p.isLinked());
    menu.addItem (7, "Unlink (keep the songs)", p.isLinked());
    menu.addItem (8, "Export as .m3u8...", ! p.songs.empty());
    menu.addSeparator();
    menu.addItem (9, "Delete playlist", show.playlists.size() > 1);

    juce::Component::SafePointer<RKPlayEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, playlist, playing] (int result)
    {
        if (safe == nullptr)
            return;
        auto& pl = safe->player;
        switch (result)
        {
            case 1:
                if (playing)
                    pl.switchPlaylist (playlist, true);
                else
                {
                    pl.switchPlaylist (playlist, true);
                    pl.play();
                }
                break;
            case 2: pl.switchPlaylist (playlist, false); break;
            case 3: safe->renamePlaylist (playlist); break;
            case 4: safe->chooseLink (playlist, true); break;
            case 5: safe->chooseLink (playlist, false); break;
            case 6: pl.rescan (playlist); break;
            case 7: pl.unlinkPlaylist (playlist); break;
            case 8: safe->exportPlaylist (playlist); break;
            case 9:
                pl.removePlaylist (playlist);
                safe->viewed = juce::jlimit (0, (int) pl.getShow().playlists.size() - 1, safe->viewed);
                break;
            default: return;
        }
        safe->refresh();
    });
}

void RKPlayEditor::renamePlaylist (int playlist)
{
    auto* window = new juce::AlertWindow ("Rename playlist", {}, juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", player.getShow().playlists[(size_t) playlist].name);
    window->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<RKPlayEditor> safe (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window, playlist] (int result)
    {
        if (safe != nullptr && result == 1)
        {
            safe->player.renamePlaylist (playlist, window->getTextEditorContents ("name"));
            safe->refresh();
        }
    }), true);
}

//==============================================================================
// The songs

int RKPlayEditor::getNumRows()
{
    return (int) viewedList().songs.size();
}

void RKPlayEditor::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto& songs = viewedList().songs;
    if (row < 0 || row >= (int) songs.size())
        return;
    const auto& song = songs[(size_t) row];
    const auto& item = player.getCurrent();
    const auto stopped = player.getState() == RKPlayProcessor::State::stopped;
    const auto playing = ! stopped && item.isValid() && ! item.queued && item.playlist == viewed && item.index == row;
    const auto cursor = stopped && viewed == player.getShow().active && nextItem.isValid() && ! nextItem.queued && nextItem.index == row;
    const auto next = ! stopped && nextItem.isValid() && ! nextItem.queued && nextItem.playlist == viewed && nextItem.index == row;

    if (selected)
        g.fillAll (rkfx::Palette::knobBody);
    if (playing || cursor)
    {
        g.setColour (accent().withAlpha (playing ? 0.25f : 0.12f));
        g.fillRect (0, 0, width, height);
    }

    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (8, 0);
    const auto colour = ! song.isPlayable() ? kProblem : playing ? rkfx::Palette::text : rkfx::Palette::text.withAlpha (0.85f);

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
        const auto plan = player.levelOf (song, viewed);
        if (! song.analysis.valid && player.getShow().level.match)
        {
            g.setColour (rkfx::Palette::textDim.withAlpha (0.6f));
            g.drawText ("measuring", levelArea, juce::Justification::centredRight);
        }
        else if (plan.limitDb > 0.0)
        {
            g.setColour (rkfx::Palette::warning);
            g.drawText ("lift " + dbText (plan.limitDb), levelArea, juce::Justification::centredRight);
        }
        else if (! juce::exactlyEqual (plan.gainDb, 0.0))
        {
            g.setColour (rkfx::Palette::textDim);
            g.drawText (dbText (plan.gainDb), levelArea, juce::Justification::centredRight);
        }
    }

    g.setColour (song.isPlayable() ? rkfx::Palette::textDim : colour);
    const auto statusText = song.status == PlaySong::Status::missing ? juce::String ("missing")
                          : song.status == PlaySong::Status::unreadable ? juce::String ("unreadable")
                          : next ? juce::String ("next   ") + timeText (song.lengthSeconds) : timeText (song.lengthSeconds);
    g.drawText (statusText, right, juce::Justification::centredRight);
}

void RKPlayEditor::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    player.jumpTo (viewed, row);
    refresh();
}

void RKPlayEditor::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showSongMenu (row);
}

void RKPlayEditor::deleteKeyPressed (int row)
{
    player.removeSong (viewed, row);
    refresh();
}

void RKPlayEditor::showSongMenu (int row)
{
    const auto& p = viewedList();
    const auto& songs = p.songs;
    if (row < 0 || row >= (int) songs.size())
        return;
    const auto& item = player.getCurrent();
    const auto playingThis = item.isValid() && ! item.queued && item.playlist == viewed && item.index == row
                          && player.getState() != RKPlayProcessor::State::stopped;
    const auto playable = songs[(size_t) row].isPlayable();

    juce::PopupMenu menu;
    menu.addItem (1, "Play now (then back to the playlist)", playable);
    menu.addItem (2, "Play next (add to the queue)", playable);
    menu.addItem (3, "Go on from here", playable);
    menu.addSeparator();
    menu.addItem (4, "Move up", row > 0 && ! p.isLinked());
    menu.addItem (5, "Move down", row < (int) songs.size() - 1 && ! p.isLinked());
    menu.addItem (6, "Remove", ! playingThis && ! p.isLinked());
    juce::PopupMenu gainMenu;
    const auto gainNow = songs[(size_t) row].gainDb;
    for (int db = 6; db >= -12; --db)
        gainMenu.addItem (100 + db + 12, (db > 0 ? "+" : "") + juce::String (db) + " dB", true, std::abs (gainNow - db) < 0.01);
    menu.addSubMenu ("Song gain", gainMenu);
    menu.addSeparator();
    menu.addItem (7, "Show in Finder", songs[(size_t) row].file.exists());

    const auto playlist = viewed;
    juce::Component::SafePointer<RKPlayEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, row, playlist] (int result)
    {
        if (safe == nullptr)
            return;
        auto& pl = safe->player;
        switch (result)
        {
            case 1: pl.playNow (playlist, row); break;
            case 2: pl.queueNext (playlist, row); break;
            case 3: pl.jumpTo (playlist, row); break;
            case 4: pl.moveSong (playlist, row, row - 1); safe->list.selectRow (row - 1); break;
            case 5: pl.moveSong (playlist, row, row + 1); safe->list.selectRow (row + 1); break;
            case 6: pl.removeSong (playlist, row); break;
            case 7: pl.getShow().playlists[(size_t) playlist].songs[(size_t) row].file.revealToUser(); break;
            default:
                if (result >= 100)
                    pl.setSongGain (playlist, row, result - 100 - 12);
                else
                    return;
        }
        safe->refresh();
    });
}

//==============================================================================
// The queue

int RKPlayEditor::QueueModel::getNumRows()
{
    return (int) owner.player.getQueue().size();
}

void RKPlayEditor::QueueModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto& queue = owner.player.getQueue();
    if (row < 0 || row >= (int) queue.size())
        return;
    if (selected)
        g.fillAll (rkfx::Palette::knobBody);
    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);
    g.setFont (rkfx::Fonts::regular (12.0f));
    g.setColour (rkfx::Palette::textDim);
    g.drawText (juce::String (row + 1), r.removeFromLeft (20), juce::Justification::centredLeft);
    g.setFont (rkfx::Fonts::regular (14.0f));
    g.setColour (rkfx::Palette::text);
    g.drawFittedText (queue[(size_t) row].song.displayName(), r, juce::Justification::centredLeft, 1);
}

void RKPlayEditor::QueueModel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;
    const auto size = (int) owner.player.getQueue().size();
    juce::PopupMenu menu;
    menu.addItem (1, "Move up", row > 0);
    menu.addItem (2, "Move down", row < size - 1);
    menu.addItem (3, "Remove from the queue");
    juce::Component::SafePointer<RKPlayEditor> safe (&owner);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, row] (int result)
    {
        if (safe == nullptr)
            return;
        if (result == 1) safe->player.moveQueued (row, row - 1);
        if (result == 2) safe->player.moveQueued (row, row + 1);
        if (result == 3) safe->player.removeQueued (row);
        safe->refresh();
    });
}

void RKPlayEditor::QueueModel::deleteKeyPressed (int row)
{
    owner.player.removeQueued (row);
    owner.refresh();
}

//==============================================================================
// End at

void RKPlayEditor::applyEndAt()
{
    const auto parts = juce::StringArray::fromTokens (endAtTime.getText(), ":", {});
    const auto hours = juce::jlimit (0, 23, parts[0].getIntValue());
    const auto minutes = juce::jlimit (0, 59, parts.size() > 1 ? parts[1].getIntValue() : 0);
    player.setEndAt (endAtOn.getToggleState(), hours * 60 + minutes, endAtMode.getSelectedId() == 2);
    refresh();
}

//==============================================================================
// Settings

RKPlayEditor::Settings::Settings (RKPlayEditor& o) : owner (o)
{
    auto& p = owner.player;
    setUpSlider (fadeOutTime, 0.5, 30.0, 0.5, " s", 56);
    fadeOutTime.setTooltip ("How long the Fade out button takes");
    fadeOutTime.onValueChange = [this, &p] { p.setFadeOut (fadeOutTime.getValue()); };
    setUpSlider (target, -23.0, -14.0, 0.5, " LUFS", 90);
    target.setTooltip ("Every song plays this loud (integrated loudness); -18 LUFS is the archive's standard");
    target.onValueChange = [this, &p] { p.setTarget (target.getValue()); };

    smart.setTooltip ("Skip the silence at songs' starts and ends, and bring the next song in where a long fade-out has gone quiet");
    match.setTooltip ("Play every song at the same loudness, by gain alone (measured once per song)");
    safety.setTooltip ("A limiter on the output at -0.3 dBFS, for overlaps the analysis can't foresee");
    smart.onClick = [this, &p] { p.setSmartCrossfade (smart.getToggleState()); };
    match.onClick = [this, &p] { p.setLevelMatch (match.getToggleState()); };
    safety.onClick = [this, &p] { p.setSafetyLimiter (safety.getToggleState()); };

    lift.addItem ("Off", 1);
    lift.addItem ("Up to 3 dB", 2);
    lift.addItem ("Up to 6 dB", 3);
    lift.setTooltip ("Lift songs too dynamic to reach the target by gain: their peaks are limited by at most this much");
    lift.onChange = [this, &p] { p.setLift (lift.getSelectedId() == 3 ? 6.0 : lift.getSelectedId() == 2 ? 3.0 : 0.0); };

    chooseFolder.setTooltip ("Songs inside this folder are kept relative to it: the show works wherever the folder is");
    chooseFolder.onClick = [this]
    {
        owner.chooser = std::make_unique<juce::FileChooser> ("The Music folder", owner.player.getShow().musicFolder);
        juce::Component::SafePointer<RKPlayEditor> safe (&owner);
        owner.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [safe] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult() != juce::File())
            {
                safe->player.setMusicFolder (fc.getResult());
                safe->refresh();
            }
        });
    };
    clearFolder.onClick = [this] { owner.player.setMusicFolder ({}); owner.refresh(); };
    relink.setTooltip ("Find missing songs in a folder (by file name, checked by size)");
    relink.onClick = [this]
    {
        owner.chooser = std::make_unique<juce::FileChooser> ("Where the missing songs are now", owner.player.getShow().musicFolder);
        juce::Component::SafePointer<RKPlayEditor> safe (&owner);
        owner.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [safe] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult() != juce::File())
            {
                safe->player.relink (fc.getResult());
                safe->refresh();
            }
        });
    };
    close.onClick = [this] { owner.showSettings (false); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &fadeOutTime, &target, &smart, &match, &safety, &lift,
                                                                          &chooseFolder, &clearFolder, &relink, &close })
        addAndMakeVisible (*c);
}

void RKPlayEditor::Settings::resized()
{
    auto r = getLocalBounds().reduced (16);
    close.setBounds (r.removeFromBottom (32).removeFromRight (110));
    r.removeFromBottom (10);
    const auto third = (r.getWidth() - 2 * kGap) / 3;
    transportArea = r.removeFromLeft (third);
    r.removeFromLeft (kGap);
    loudnessArea = r.removeFromLeft (third);
    r.removeFromLeft (kGap);
    folderArea = r;

    auto t = transportArea.reduced (14, 10).withTrimmedTop (30);
    t.removeFromTop (18);
    fadeOutTime.setBounds (t.removeFromTop (28));
    t.removeFromTop (14);
    smart.setBounds (t.removeFromTop (28));

    auto l = loudnessArea.reduced (14, 10).withTrimmedTop (30);
    match.setBounds (l.removeFromTop (28));
    l.removeFromTop (22);
    target.setBounds (l.removeFromTop (28));
    l.removeFromTop (22);
    lift.setBounds (l.removeFromTop (28).withTrimmedRight (60));
    l.removeFromTop (12);
    safety.setBounds (l.removeFromTop (28));

    auto f = folderArea.reduced (14, 10).withTrimmedTop (30);
    f.removeFromTop (60);
    auto row = f.removeFromTop (30);
    chooseFolder.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    clearFolder.setBounds (row.removeFromLeft (70));
    f.removeFromTop (30);
    relink.setBounds (f.removeFromTop (30));
}

void RKPlayEditor::Settings::paint (juce::Graphics& g)
{
    g.setColour (owner.backgroundColour());
    g.fillRoundedRectangle (getLocalBounds().toFloat(), rkfx::Style::kPanelRadius);
    owner.drawSection (g, transportArea, "Playback");
    owner.drawSection (g, loudnessArea, "Loudness");
    owner.drawSection (g, folderArea, "Music folder");

    g.setColour (rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (13.0f));
    const auto label = [&] (juce::Component& c, const juce::String& text)
    {
        g.drawText (text, c.getBounds().withY (c.getY() - 18).withHeight (16), juce::Justification::centredLeft);
    };
    label (fadeOutTime, "Fade out");
    label (target, "Target");
    label (lift, "Lift quiet songs");

    const auto& folder = owner.player.getShow().musicFolder;
    auto text = folderArea.reduced (14, 10).withTrimmedTop (30).removeFromTop (54);
    g.setColour (folder != juce::File() ? rkfx::Palette::text : rkfx::Palette::textDim);
    g.setFont (rkfx::Fonts::regular (14.0f));
    g.drawFittedText (folder != juce::File() ? folder.getFullPathName() : juce::String ("None: every song by its full path"),
                      text, juce::Justification::topLeft, 3);
    const auto problems = owner.player.getShow().numProblems();
    g.setColour (problems > 0 ? kProblem : rkfx::Palette::textDim);
    g.drawText (problems > 0 ? juce::String (problems) + (problems == 1 ? " song is" : " songs are") + " missing or unreadable"
                             : juce::String ("Every song is in place"),
                relink.getBounds().translated (0, -26).withHeight (20), juce::Justification::centredLeft);
}

void RKPlayEditor::Settings::refresh()
{
    const auto& show = owner.player.getShow();
    fadeOutTime.setValue (show.fadeOutSeconds, juce::dontSendNotification);
    target.setValue (show.level.targetLufs, juce::dontSendNotification);
    smart.setToggleState (show.smartCrossfade, juce::dontSendNotification);
    match.setToggleState (show.level.match, juce::dontSendNotification);
    safety.setToggleState (show.safetyLimiter, juce::dontSendNotification);
    lift.setSelectedId (show.level.liftDb > 4.5 ? 3 : show.level.liftDb > 0.5 ? 2 : 1, juce::dontSendNotification);
    target.setEnabled (show.level.match);
    lift.setEnabled (show.level.match);
    clearFolder.setEnabled (show.musicFolder != juce::File());
    repaint();
}

void RKPlayEditor::showSettings (bool shown)
{
    settings.setVisible (shown);
    if (shown)
        settings.toFront (false);
    settingsButton.setToggleState (shown, juce::dontSendNotification);
    refresh();
}

//==============================================================================
// Files

void RKPlayEditor::filesDropped (const juce::StringArray& paths, int x, int y)
{
    dropHighlight = false;
    juce::Array<juce::File> files;
    for (const auto& path : paths)
        files.add (juce::File (path));

    // Onto a row: before it; anywhere else: at the end. A linked list takes nothing.
    if (viewedList().isLinked())
    {
        repaint();
        return;
    }
    const auto inList = list.getLocalPoint (this, juce::Point<int> (x, y));
    auto row = list.getLocalBounds().contains (inList) ? list.getRowContainingPosition (inList.x, inList.y) : -1;
    player.addFiles (viewed, files, row);
    refresh();
}

void RKPlayEditor::chooseFiles (bool folders)
{
    chooser = std::make_unique<juce::FileChooser> (folders ? "Add a folder of songs" : "Add songs or .m3u8 playlists",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   folders ? juce::String() : player.getFormats().getWildcardForAllFormats() + ";*.m3u;*.m3u8");
    const auto flags = juce::FileBrowserComponent::openMode
                     | (folders ? juce::FileBrowserComponent::canSelectDirectories
                                : juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems);
    const auto playlist = viewed;
    juce::Component::SafePointer<RKPlayEditor> safe (this);
    chooser->launchAsync (flags, [safe, playlist] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResults().isEmpty())
            return;
        safe->player.addFiles (playlist, fc.getResults());
        safe->refresh();
    });
}

void RKPlayEditor::chooseLink (int playlist, bool folder)
{
    chooser = std::make_unique<juce::FileChooser> (folder ? "Link the playlist to a folder" : "Link the playlist to an .m3u8 file",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   folder ? juce::String() : "*.m3u;*.m3u8");
    const auto flags = juce::FileBrowserComponent::openMode
                     | (folder ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles);
    juce::Component::SafePointer<RKPlayEditor> safe (this);
    chooser->launchAsync (flags, [safe, playlist] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult() != juce::File())
        {
            safe->player.linkPlaylist (playlist, fc.getResult());
            safe->refresh();
        }
    });
}

void RKPlayEditor::exportPlaylist (int playlist)
{
    const auto& name = player.getShow().playlists[(size_t) playlist].name;
    chooser = std::make_unique<juce::FileChooser> ("Export the playlist",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (name + ".m3u8"),
                                                   "*.m3u8");
    juce::Component::SafePointer<RKPlayEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, playlist] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult() != juce::File())
            safe->player.exportPlaylist (playlist, fc.getResult().withFileExtension ("m3u8"));
    });
}

//==============================================================================
void RKPlayEditor::refresh()
{
    lastShowVersion = player.getShowVersion();
    const auto& show = player.getShow();
    viewed = juce::jlimit (0, (int) show.playlists.size() - 1, viewed);
    nextItem = player.peekNext();

    const auto& p = viewedList();
    inOrderButton.setToggleState (p.order == PlayPlaylist::Order::inOrder, juce::dontSendNotification);
    shuffleButton.setToggleState (p.order == PlayPlaylist::Order::shuffle, juce::dontSendNotification);
    atEnd.clear (juce::dontSendNotification);
    atEnd.addItem ("At the end: stop", 1);
    atEnd.addItem ("At the end: repeat", 2);
    for (int i = 0; i < (int) show.playlists.size(); ++i)
        if (i != viewed)
            atEnd.addItem ("Then: " + show.playlists[(size_t) i].name, 10 + i);
    atEnd.setSelectedId (p.atEnd == PlayPlaylist::AtEnd::stop ? 1 : p.atEnd == PlayPlaylist::AtEnd::repeat ? 2 : 10 + p.goTo,
                         juce::dontSendNotification);
    stopAfterEach.setToggleState (p.stopAfterEach, juce::dontSendNotification);
    crossfade.setValue (p.crossfadeSeconds, juce::dontSendNotification);
    listGain.setValue (p.gainDb, juce::dontSendNotification);
    for (auto* b : { &addFilesButton, &addFolderButton, &clearButton })
        b->setEnabled (! p.isLinked() || b == &clearButton);

    if (! endAtTime.hasKeyboardFocus (true))
        endAtTime.setText (juce::String (show.endAt.minutes / 60).paddedLeft ('0', 2) + ":" + juce::String (show.endAt.minutes % 60).paddedLeft ('0', 2),
                           juce::dontSendNotification);
    endAtOn.setToggleState (show.endAt.on, juce::dontSendNotification);
    endAtMode.setSelectedId (show.endAt.atSongEnd ? 2 : 1, juce::dontSendNotification);

    const auto state = player.getState();
    playButton.setButtonText (state == RKPlayProcessor::State::playing ? "Pause" : "Play");
    playButton.setToggleState (state == RKPlayProcessor::State::playing, juce::dontSendNotification);

    list.updateContent();
    list.repaint();
    queueList.updateContent();
    queueList.repaint();
    queueList.setVisible (! player.getQueue().empty());   // empty: the hint shows instead
    clearQueueButton.setEnabled (! player.getQueue().empty());
    settings.refresh();
    tabs.repaint();
    nowPlaying.repaint();
    repaint();
}

void RKPlayEditor::tick()
{
    if (player.getShowVersion() != lastShowVersion)
        refresh();
    else
    {
        nowPlaying.repaint();
        if (player.getShow().endAt.on)
            repaint (stopButton.getBounds().withRight (settingsButton.getX()));
    }

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
    const auto h = total / 3600;
    return (h > 0 ? juce::String (h) + ":" + juce::String ((total / 60) % 60).paddedLeft ('0', 2) : juce::String (total / 60))
           + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
