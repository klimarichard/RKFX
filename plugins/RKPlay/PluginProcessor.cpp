#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKPlay";
        c.stateVersion = 1;
        c.hasMix = false;
        c.hasOutput = false;                  // its own Level (a host parameter)
        c.midiInput = true;                   // MIDI learn and program changes (later phases)
        c.extraInPresets = true;              // a preset is the whole show
        c.bypassMutes = true;                 // the input is only the duck key: never passed on
        c.auxOutput = "Cue";                  // pre-listen (headphones)
        c.accent = juce::Colour (0xff6fa58a); // Utility family, sage
        c.family = rkfx::Palette::Family::utility;
        return c;
    }

    rkfx::ParamBuilder makeParams()
    {
        rkfx::ParamBuilder p;
        // Transport triggers: a widget or footswitch turns one on, RKPlay acts and turns it off.
        p.addBool ("play", "Play", false);
        p.addBool ("pause", "Pause", false);
        p.addBool ("next", "Next", false);
        p.addBool ("prev", "Previous", false);
        p.addBool ("fade", "Fade Out", false);
        p.addBool ("stop", "Stop", false);
        p.addDecibels ("level", "Level", -60.0f, 6.0f, 0.0f);   // -60 = silence
        // Appended (phase 3): the playing playlist; changing it crossfades into that list.
        p.addInt ("playlist", "Playlist", 1, PlayShow::kMaxPlaylists, 1);
        // Appended (phase 4): Duck (latching), and the pads (triggers).
        p.addBool ("duck", "Duck", false);
        for (int pad = 1; pad <= PlayEngine::kPads; ++pad)
            p.addBool ("pad" + juce::String (pad), "Pad " + juce::String (pad), false);
        return p;
    }

    constexpr double kStopFade = 0.1, kPanicFade = 0.02, kSeekFade = 0.05, kRestartFade = 0.3;
    constexpr double kPrefillSeconds = 1.0;    // read at once when a song is opened
    constexpr double kRestartAfter = 5.0;      // Previous restarts the song after this much of it
}

RKPlayProcessor::RKPlayProcessor()
    : rkfx::ProcessorBase (makeConfig(), makeParams())
{
    formats.registerBasicFormats();
    const char* ids[] = { "play", "pause", "next", "prev", "fade", "stop" };
    for (size_t i = 0; i < triggers.size(); ++i)
        triggers[i] = findParam (ids[i]);
    levelParam = findParam ("level");
    playlistParam = findParam ("playlist");
    duckParam = findParam ("duck");
    for (int pad = 0; pad < PlayEngine::kPads; ++pad)
        padParams[(size_t) pad] = findParam ("pad" + juce::String (pad + 1));
    cursorPlayed.assign (show.playlists.size(), false);

    for (int slot = 0; slot < PlayEngine::kSlots; ++slot)
        engine.setStream (slot, &streams[(size_t) slot]);
    diskThread.startThread (juce::Thread::Priority::high);
    cache = std::make_unique<PlayAnalysisCache> (PlayAnalysisCache::defaultFile());
    analyser = std::make_unique<PlayAnalyser>();
    if (wrapperType == wrapperType_Standalone)
        rig = std::make_unique<PlayRig> (*this, PlayRig::defaultFolder());
    startTimerHz (30);
}

RKPlayProcessor::~RKPlayProcessor()
{
    stopTimer();
    if (rig != nullptr)
    {
        finishCurrent();
        rig->writeSession (true);   // a normal quit: nothing to recover
        rig.reset();
    }
    analyser.reset();
    if (cache != nullptr)
        cache->save();
    for (auto& stream : streams)
        diskThread.removeTimeSliceClient (&stream);
    diskThread.stopThread (2000);
}

//==============================================================================
void RKPlayProcessor::prepareDsp (double newRate, int maxBlockSize, int /*numChannels*/)
{
    monoScratch.setSize (2, maxBlockSize);
    keyBuffer.setSize (2, maxBlockSize);

    // Same rate: keep playing (a host may prepare again for a new block size).
    if (juce::exactlyEqual (newRate, sampleRate))
    {
        engine.prepareBlock (maxBlockSize);
        return;
    }

    // A new rate: the streams resample for it, so everything starts again.
    for (auto& stream : streams)
        diskThread.removeTimeSliceClient (&stream);
    sampleRate = newRate;
    for (auto& stream : streams)
        stream.prepare (sampleRate);
    engine.prepare (sampleRate, maxBlockSize);
    slots = {};
    currentSlot = armedSlot = cueSlot = -1;
    current = armed = {};
    state = State::stopped;
    padsDirty = true;   // rendered again at the new rate (message thread)
}

void RKPlayProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    // The standalone app: controllers and program changes, to the message thread.
    if (rig != nullptr)
        if (const auto* midi = getMidi())
            for (const auto metadata : *midi)
            {
                const auto m = metadata.getMessage();
                if (! (m.isController() || m.isProgramChange()))
                    continue;
                int start1, size1, start2, size2;
                midiFifo.prepareToWrite (1, start1, size1, start2, size2);
                if (size1 + size2 == 0)
                    break;
                const auto* raw = m.getRawData();
                midiEvents[(size_t) (size1 > 0 ? start1 : start2)] = (juce::uint32) raw[0] | ((juce::uint32) raw[1] << 8)
                                                                     | ((juce::uint32) (m.getRawDataSize() > 2 ? raw[2] : 0) << 16);
                midiFifo.finishedWrite (1);
            }

    // The input is only ever the duck key: the output is the music alone.
    const auto n = buffer.getNumSamples();
    const auto keyChannels = juce::jmin (2, getMainBusNumInputChannels(), buffer.getNumChannels());
    for (int ch = 0; ch < keyChannels; ++ch)
        keyBuffer.copyFrom (ch, 0, buffer, ch, 0, n);
    buffer.clear();
    if (sampleRate <= 0.0)
        return;
    const float* key[2] = { keyBuffer.getReadPointer (0), keyBuffer.getReadPointer (1) };
    auto* cue = getAuxOutput();
    float* const* cuePointers = cue != nullptr && cue->getNumChannels() >= 2 ? cue->getArrayOfWritePointers() : nullptr;
    if (buffer.getNumChannels() >= 2)
    {
        engine.process (buffer.getArrayOfWritePointers(), n, keyChannels > 0 ? key : nullptr, keyChannels, cuePointers);
        return;
    }
    monoScratch.clear (0, n);
    engine.process (monoScratch.getArrayOfWritePointers(), n, keyChannels > 0 ? key : nullptr, keyChannels, cuePointers);
    buffer.copyFrom (0, 0, monoScratch, 0, 0, n);
    buffer.addFrom (0, 0, monoScratch, 1, 0, n);
    buffer.applyGain (0, 0, n, 0.5f);
}

//==============================================================================
// Slots

int RKPlayProcessor::load (const Item& item, double startSeconds)
{
    if (! item.isValid() || ! item.song.isPlayable() || sampleRate <= 0.0)
        return -1;

    int slot = -1;
    for (int s = 0; s < PlayEngine::kSlots && slot < 0; ++s)
        if (! slots[(size_t) s].loaded && engine.isSlotIdle (s, slots[(size_t) s].lastCommand))
            slot = s;
    if (slot < 0)
        return -1;   // every stream busy (a burst of skips): the next tick tries again

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (item.song.file));
    if (reader == nullptr)
    {
        // Gone since the show was checked: marked wherever it is, and skipped.
        const auto status = item.song.file.existsAsFile() ? PlaySong::Status::unreadable : PlaySong::Status::missing;
        for (auto& p : show.playlists)
            for (auto& s : p.songs)
                if (s.file == item.song.file)
                    s.status = status;
        setNote ("Skipped " + item.song.displayName() + ": the file is " + (status == PlaySong::Status::missing ? "missing" : "unreadable"));
        showChanged();
        return -1;
    }

    const auto options = optionsFor (item, startSeconds);
    auto& stream = streams[(size_t) slot];
    diskThread.removeTimeSliceClient (&stream);
    stream.open (std::move (reader), options);
    if (synchronousDisk)
    {
        stream.fillNow();
    }
    else
    {
        stream.prefill (kPrefillSeconds);
        diskThread.addTimeSliceClient (&stream);
    }
    auto& entry = slots[(size_t) slot];
    entry.loaded = true;
    entry.lastCommand = engine.commandsSent();
    entry.options = options;
    entry.song = item.song;
    return slot;
}

PlayLevel RKPlayProcessor::levelOf (const PlaySong& song, int playlist) const
{
    const auto listGain = playlist >= 0 && playlist < (int) show.playlists.size() ? show.playlists[(size_t) playlist].gainDb : 0.0;
    return PlayLevel::plan (song.analysis, show.level, song.gainDb + listGain);
}

PlayStream::Options RKPlayProcessor::optionsFor (const Item& item, double startSeconds) const
{
    const auto span = show.spanOf (item.song);
    const auto level = levelOf (item.song, item.playlist);
    PlayStream::Options o;
    o.startSeconds = startSeconds < 0.0 ? span.start : startSeconds;
    o.endSeconds = item.song.analysis.valid && show.smartCrossfade ? span.end : -1.0;
    o.gainDb = level.gainDb;
    o.limitCeilingDb = level.limitDb > 0.0 ? show.level.peakCeilingDb : 1.0;
    return o;
}

double RKPlayProcessor::leadOf (int slot) const
{
    // Smart crossfade: the song is over at its fade-out point, before the stream's end.
    if (slot < 0 || ! show.smartCrossfade)
        return 0.0;
    const auto& entry = slots[(size_t) slot];
    if (! entry.song.analysis.valid)
        return 0.0;
    const auto streamEnd = entry.options.endSeconds < 0.0 ? entry.song.lengthSeconds : entry.options.endSeconds;
    return juce::jmax (0.0, streamEnd - entry.song.analysis.fadeOutPoint);
}

void RKPlayProcessor::drop (int& slot)
{
    if (slot >= 0)
        slots[(size_t) slot].loaded = false;
    slot = -1;
}

void RKPlayProcessor::disarm()
{
    if (armedSlot >= 0)
    {
        engine.disarm();
        markSent (armedSlot);
    }
    drop (armedSlot);
    armed = {};
    armedCrossfade = -1.0;
    armedLead = 0.0;
}

//==============================================================================
// What plays next

RKPlayProcessor::Item RKPlayProcessor::itemAt (int playlist, int index) const
{
    Item item;
    if (playlist < 0 || playlist >= (int) show.playlists.size())
        return item;
    const auto& p = show.playlists[(size_t) playlist];
    if (index < 0 || index >= (int) p.songs.size())
        return item;
    item.song = p.songs[(size_t) index];
    item.playlist = playlist;
    item.index = index;
    return item;
}

void RKPlayProcessor::setPlayed (int playlist, bool played)
{
    if (playlist < 0)
        return;
    if ((int) cursorPlayed.size() < (int) show.playlists.size())
        cursorPlayed.resize (show.playlists.size(), false);
    cursorPlayed[(size_t) playlist] = played;
}

void RKPlayProcessor::ensureBag (int playlist)
{
    auto& p = show.playlists[(size_t) playlist];
    if (p.order != PlayPlaylist::Order::shuffle || p.bag.size() == p.songs.size())
        return;
    PlayShow::reshuffle (p, -1, random);
    p.nextBag.clear();
    // A list already playing: the song now playing counts as this round's first.
    if (isPlayed (playlist))
        if (const auto it = std::find (p.bag.begin(), p.bag.end(), p.cursor); it != p.bag.end())
        {
            std::iter_swap (p.bag.begin(), it);
            p.bagPos = 0;
        }
}

RKPlayProcessor::Item RKPlayProcessor::following (int playlist, int depth)
{
    if (playlist < 0 || playlist >= (int) show.playlists.size() || depth > PlayShow::kMaxPlaylists)
        return {};
    auto& p = show.playlists[(size_t) playlist];
    ensureBag (playlist);

    // Not started yet: its cursor song (the first of a shuffle round), if it plays.
    if (! isPlayed (playlist) && ! p.songs.empty())
    {
        const auto first = p.order == PlayPlaylist::Order::shuffle && p.bagPos < 0 && ! p.bag.empty() ? p.bag.front() : p.cursor;
        if (first >= 0 && first < (int) p.songs.size() && p.songs[(size_t) first].isPlayable())
            return itemAt (playlist, first);
        return stepFrom (playlist, first, depth);
    }
    return stepFrom (playlist, p.cursor, depth);
}

RKPlayProcessor::Item RKPlayProcessor::stepFrom (int playlist, int index, int depth)
{
    auto& p = show.playlists[(size_t) playlist];
    const auto n = (int) p.songs.size();
    const auto playable = [&p] (int i) { return i >= 0 && i < (int) p.songs.size() && p.songs[(size_t) i].isPlayable(); };

    if (p.order == PlayPlaylist::Order::inOrder)
    {
        for (int i = index + 1; i < n; ++i)
            if (playable (i))
                return itemAt (playlist, i);
    }
    else
    {
        ensureBag (playlist);
        auto pos = p.bagPos;
        if (pos < 0 || pos >= (int) p.bag.size() || p.bag[(size_t) pos] != index)
        {
            const auto it = std::find (p.bag.begin(), p.bag.end(), index);
            pos = it != p.bag.end() ? (int) (it - p.bag.begin()) : -1;
        }
        for (int k = pos + 1; k < (int) p.bag.size(); ++k)
            if (playable (p.bag[(size_t) k]))
                return itemAt (playlist, p.bag[(size_t) k]);
    }

    // The end of the list (or of the shuffle round).
    switch (p.atEnd)
    {
        case PlayPlaylist::AtEnd::stop:
            return {};

        case PlayPlaylist::AtEnd::goTo:
            if (p.goTo != playlist && p.goTo >= 0 && p.goTo < (int) show.playlists.size())
                return following (p.goTo, depth + 1);
            [[fallthrough]];

        case PlayPlaylist::AtEnd::repeat:
            if (p.order == PlayPlaylist::Order::inOrder)
            {
                for (int i = 0; i <= juce::jmin (index, n - 1); ++i)
                    if (playable (i))
                        return itemAt (playlist, i);
                return {};
            }
            // Shuffle: the next round's order (made once, kept until it starts).
            if (p.nextBag.size() != p.songs.size())
            {
                auto copy = p;
                PlayShow::reshuffle (copy, index, random);
                p.nextBag = copy.bag;
            }
            for (const auto i : p.nextBag)
                if (playable (i))
                    return itemAt (playlist, i);
            return {};
    }
    return {};
}

RKPlayProcessor::Item RKPlayProcessor::previousOf (const Item& item)
{
    const auto playlist = item.isValid() && ! item.queued ? item.playlist : show.active;
    if (playlist < 0 || playlist >= (int) show.playlists.size())
        return {};
    auto& p = show.playlists[(size_t) playlist];
    const auto index = item.isValid() && ! item.queued ? item.index : p.cursor;
    if (item.queued && isPlayed (playlist))
        return itemAt (playlist, p.cursor);   // a request: back to the playlist's song
    const auto playable = [&p] (int i) { return i >= 0 && i < (int) p.songs.size() && p.songs[(size_t) i].isPlayable(); };

    if (p.order == PlayPlaylist::Order::inOrder)
    {
        for (int i = index - 1; i >= 0; --i)
            if (playable (i))
                return itemAt (playlist, i);
        return {};
    }
    ensureBag (playlist);
    const auto it = std::find (p.bag.begin(), p.bag.end(), index);
    for (auto k = (int) (it - p.bag.begin()) - 1; k >= 0; --k)
        if (playable (p.bag[(size_t) k]))
            return itemAt (playlist, p.bag[(size_t) k]);
    return {};
}

RKPlayProcessor::Item RKPlayProcessor::peekNext()
{
    if (endingAtSongEnd)
        return {};
    // Stop after each song (the playing list's setting): nothing follows by itself.
    if (state != State::stopped && show.current().stopAfterEach)
        return {};
    if (! queue.empty())
        return queue.front();
    if (pendingSwitch >= 0)
        return following (pendingSwitch);
    return following (show.active);
}

void RKPlayProcessor::commit (const Item& item)
{
    if (item.queued)
    {
        // A request: off the queue; the playlist stays where it was.
        for (auto it = queue.begin(); it != queue.end(); ++it)
            if (it->queueId == item.queueId && item.queueId != 0)
            {
                queue.erase (it);
                break;
            }
        return;
    }

    const auto playlist = item.playlist;
    auto& p = show.playlists[(size_t) playlist];
    p.cursor = item.index;
    if (p.order == PlayPlaylist::Order::shuffle)
    {
        ensureBag (playlist);
        if (const auto it = std::find (p.bag.begin() + juce::jmax (0, p.bagPos + 1), p.bag.end(), item.index); it != p.bag.end())
        {
            p.bagPos = (int) (it - p.bag.begin());
        }
        else if (const auto inNext = std::find (p.nextBag.begin(), p.nextBag.end(), item.index); inNext != p.nextBag.end())
        {
            // A new round.
            const auto position = (int) (inNext - p.nextBag.begin());
            p.bag = std::exchange (p.nextBag, {});
            p.bagPos = position;
        }
        else if (const auto anywhere = std::find (p.bag.begin(), p.bag.end(), item.index); anywhere != p.bag.end())
        {
            p.bagPos = (int) (anywhere - p.bag.begin());   // jumped back to one played this round
        }
    }
    setPlayed (playlist, true);
    if (show.active != playlist)
    {
        show.active = playlist;
        syncPlaylistParam();
    }
    if (pendingSwitch == playlist)
        pendingSwitch = -1;
}

double RKPlayProcessor::crossfadeInto (const Item& item) const
{
    const auto playlist = item.playlist >= 0 && item.playlist < (int) show.playlists.size() && ! item.queued ? item.playlist : show.active;
    return show.playlists[(size_t) playlist].crossfadeSeconds;
}

void RKPlayProcessor::startItem (const Item& item, double crossfade)
{
    if (! item.isValid())
        return;
    const auto slot = load (item, -1.0);
    if (slot < 0)
    {
        // Unreadable now (load said why): go on to the one after it.
        if (! item.queued && item.playlist >= 0)
        {
            const auto after = stepFrom (item.playlist, item.index, 0);
            if (after.isValid() && ! after.sameAs (item) && ! after.sameAs (current))
                startItem (after, crossfade);
        }
        return;
    }

    // Never longer than half of either song.
    for (const PlaySong* song : { &item.song, current.isValid() ? static_cast<const PlaySong*> (&current.song) : nullptr })
        if (song != nullptr)
        {
            const auto span = show.spanOf (*song);
            crossfade = juce::jmin (crossfade, (span.endPoint - span.start) * 0.5);
        }

    disarm();
    finishCurrent();
    currentStarted = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (state == State::stopped)
        engine.start (slot, 0.01, 1.0f);
    else
        engine.crossfadeTo (slot, state == State::paused ? kSeekFade : juce::jmax (0.0, crossfade), 1.0f);
    markSent (slot);
    markSent (currentSlot);
    drop (currentSlot);
    current = item;
    currentSlot = slot;
    state = State::playing;
    underran = false;
    if (cueSlot >= 0 && cueSong.file == item.song.file)
        stopPreListen();   // it's played for real now
    commit (item);
    keepArmed();
    showChanged();
}

void RKPlayProcessor::keepArmed()
{
    if (state == State::stopped || currentSlot < 0)
        return;

    const auto want = peekNext();
    if (! want.isValid())
    {
        disarm();
        return;
    }
    auto seconds = crossfadeInto (want);
    for (const PlaySong* song : { static_cast<const PlaySong*> (&current.song), &want.song })
    {
        const auto span = show.spanOf (*song);
        seconds = juce::jmin (seconds, (span.endPoint - span.start) * 0.5);
    }
    seconds = juce::jmax (0.0, seconds);
    const auto lead = leadOf (currentSlot);

    // Already armed with that song, opened as it should be now (the analysis
    // or the loudness settings may have changed how).
    if (armedSlot >= 0 && armed.sameAs (want))
    {
        const auto& have = slots[(size_t) armedSlot].options;
        const auto wantOptions = optionsFor (want, -1.0);
        const auto same = juce::approximatelyEqual (have.startSeconds, wantOptions.startSeconds)
                       && juce::approximatelyEqual (have.endSeconds, wantOptions.endSeconds)
                       && juce::approximatelyEqual (have.gainDb, wantOptions.gainDb)
                       && juce::approximatelyEqual (have.limitCeilingDb, wantOptions.limitCeilingDb);
        if (same)
        {
            if (! juce::exactlyEqual (seconds, armedCrossfade) || ! juce::exactlyEqual (lead, armedLead))
            {
                engine.arm (armedSlot, seconds, 1.0f, lead);
                markSent (armedSlot);
                armedCrossfade = seconds;
                armedLead = lead;
            }
            return;
        }
    }

    disarm();
    const auto slot = load (want, -1.0);
    if (slot < 0)
        return;
    engine.arm (slot, seconds, 1.0f, lead);
    markSent (slot);
    armed = want;
    armedSlot = slot;
    armedCrossfade = seconds;
    armedLead = lead;
}

//==============================================================================
// Transport

void RKPlayProcessor::play()
{
    if (state == State::paused)
    {
        engine.resume (show.pauseFadeSeconds);
        state = State::playing;
        showChanged();
        return;
    }
    if (state == State::playing)
        return;

    const auto item = ! queue.empty() ? queue.front() : following (show.active);
    if (! item.isValid())
    {
        setNote (show.current().songs.empty() ? "Nothing to play: the playlist is empty"
                                             : "Nothing to play: the playlist has ended (or every file is missing)");
        return;
    }
    startItem (item, 0.0);
}

void RKPlayProcessor::pause()
{
    if (state != State::playing)
        return;
    engine.pause (show.pauseFadeSeconds);
    state = State::paused;
    showChanged();
}

void RKPlayProcessor::next()
{
    if (state == State::stopped)
    {
        // Stopped: the next song becomes the one Play starts.
        const auto n = following (show.active);
        if (n.isValid())
        {
            show.playlists[(size_t) n.playlist].cursor = n.index;
            show.active = n.playlist;
            setPlayed (n.playlist, false);
            showChanged();
        }
        return;
    }
    // Stop after each: Next still goes on (by hand).
    auto n = ! queue.empty() ? queue.front() : pendingSwitch >= 0 ? following (pendingSwitch) : following (show.active);
    if (n.isValid())
        startItem (n, crossfadeInto (n));
}

void RKPlayProcessor::previous()
{
    if (state != State::stopped && getPositionSeconds() > kRestartAfter)
    {
        startItem (current, kRestartFade);
        return;
    }
    const auto p = previousOf (current.isValid() ? current : Item {});
    if (! p.isValid())
        return;
    if (state == State::stopped)
    {
        show.playlists[(size_t) p.playlist].cursor = p.index;
        setPlayed (p.playlist, false);
        showChanged();
        return;
    }
    startItem (p, crossfadeInto (p));
}

void RKPlayProcessor::fadeOut()
{
    if (state == State::stopped)
        return;
    disarm();
    finishCurrent();
    engine.fadeOutAll (show.fadeOutSeconds);
    markSent (currentSlot);
    drop (currentSlot);
    current = {};
    state = State::stopped;   // Play goes on with the next song (the list's cursor was played)
    showChanged();
}

void RKPlayProcessor::stop()
{
    if (state == State::stopped)
        return;
    disarm();
    finishCurrent();
    engine.fadeOutAll (kStopFade);
    markSent (currentSlot);
    drop (currentSlot);
    // Play starts the same song again: a request goes back to the queue's front.
    if (current.queued && current.queueId != 0)
        queue.push_front (current);
    else if (! current.queued)
        setPlayed (current.playlist, false);
    current = {};
    state = State::stopped;
    showChanged();
}

void RKPlayProcessor::panic()
{
    disarm();
    finishCurrent();
    engine.fadeOutAll (kPanicFade);
    markSent (currentSlot);
    drop (currentSlot);
    current = {};
    state = State::stopped;
    showChanged();
}

void RKPlayProcessor::jumpTo (int playlist, int index)
{
    const auto item = itemAt (playlist, index);
    if (! item.isValid() || ! item.song.isPlayable())
        return;
    startItem (item, state == State::playing ? crossfadeInto (item) : 0.0);
}

void RKPlayProcessor::playNow (int playlist, int index)
{
    auto item = itemAt (playlist, index);
    if (! item.isValid() || ! item.song.isPlayable())
        return;
    item.queued = true;   // a one-off: the playlist goes on where it was after it
    startItem (item, state == State::playing ? crossfadeInto (item) : 0.0);
}

void RKPlayProcessor::seek (double seconds)
{
    if (state == State::stopped || currentSlot < 0)
        return;
    const auto slot = load (current, seconds);
    if (slot < 0)
        return;
    disarm();
    engine.crossfadeTo (slot, kSeekFade, 1.0f);
    if (state == State::paused)
        engine.pause (0.0);   // stay paused at the new place
    markSent (slot);
    markSent (currentSlot);
    drop (currentSlot);
    currentSlot = slot;
    keepArmed();
}

void RKPlayProcessor::switchPlaylist (int playlist, bool now)
{
    if (playlist < 0 || playlist >= (int) show.playlists.size())
        return;
    if (state == State::stopped)
    {
        show.active = playlist;
        pendingSwitch = -1;
        syncPlaylistParam();
        showChanged();
        return;
    }
    if (now)
    {
        const auto item = following (playlist);
        if (item.isValid())
            startItem (item, crossfadeInto (item));
        return;
    }
    pendingSwitch = playlist == show.active ? -1 : playlist;
    keepArmed();
    showChanged();
}

double RKPlayProcessor::getPositionSeconds() const
{
    return currentSlot >= 0 ? streams[(size_t) currentSlot].getPositionSeconds() : 0.0;
}

double RKPlayProcessor::getLengthSeconds() const
{
    if (current.isValid())
        return current.song.lengthSeconds;
    const auto& p = show.current();
    return p.cursor >= 0 && p.cursor < (int) p.songs.size() ? p.songs[(size_t) p.cursor].lengthSeconds : 0.0;
}

//==============================================================================
// The queue

void RKPlayProcessor::queueNext (int playlist, int index)
{
    auto item = itemAt (playlist, index);
    if (! item.isValid())
        return;
    item.queued = true;
    item.queueId = nextQueueId++;
    queue.push_back (item);
    showChanged();
}

void RKPlayProcessor::removeQueued (int position)
{
    if (position < 0 || position >= (int) queue.size())
        return;
    queue.erase (queue.begin() + position);
    showChanged();
}

void RKPlayProcessor::moveQueued (int from, int to)
{
    const auto n = (int) queue.size();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    auto item = queue[(size_t) from];
    queue.erase (queue.begin() + from);
    queue.insert (queue.begin() + to, item);
    showChanged();
}

void RKPlayProcessor::clearQueue()
{
    queue.clear();
    showChanged();
}

//==============================================================================
// End at

void RKPlayProcessor::setEndAt (bool on, int minutes, bool atSongEnd)
{
    show.endAt = { on, juce::jlimit (0, 24 * 60 - 1, minutes), atSongEnd };
    endingAtSongEnd = false;
    findEndAtDeadline();
    edited();
}

void RKPlayProcessor::findEndAtDeadline()
{
    // The next time the clock shows it (today, or tomorrow if it has passed).
    const auto now = clock();
    const auto minutes = show.endAt.minutes;
    auto deadline = juce::Time (now.getYear(), now.getMonth(), now.getDayOfMonth(), minutes / 60, minutes % 60, 0, 0, true);
    if (deadline <= now)
        deadline += juce::RelativeTime::days (1);
    endAtDeadline = deadline;
}

double RKPlayProcessor::getEndAtRemaining() const
{
    return show.endAt.on ? (endAtDeadline - clock()).inSeconds() : -1.0;
}

void RKPlayProcessor::checkEndAt()
{
    if (! show.endAt.on || clock() < endAtDeadline)
        return;
    show.endAt.on = false;   // once
    if (state == State::stopped)
    {
        showChanged();
        return;
    }
    if (show.endAt.atSongEnd)
    {
        endingAtSongEnd = true;
        disarm();
        setNote ("End at: the music stops after this song");
    }
    else
    {
        fadeOut();
        setNote ("End at: the music faded out");
    }
    showChanged();
}

//==============================================================================
// The message thread's regular work

void RKPlayProcessor::update()
{
    if (synchronousDisk)
        for (auto& stream : streams)
            stream.fillNow();

    pollTriggers();
    if (rig != nullptr)
    {
        int start1, size1, start2, size2;
        midiFifo.prepareToRead (midiFifo.getNumReady(), start1, size1, start2, size2);
        for (int i = 0; i < size1 + size2; ++i)
        {
            const auto e = midiEvents[(size_t) (i < size1 ? start1 + i : start2 + i - size1)];
            const auto status = (int) (e & 0xff);
            rig->handleMidi ((status & 0xf0) == 0xc0 ? juce::MidiMessage ((int) (e & 0xff), (int) ((e >> 8) & 0xff))
                                                     : juce::MidiMessage ((int) (e & 0xff), (int) ((e >> 8) & 0xff), (int) ((e >> 16) & 0xff)));
        }
        midiFifo.finishedRead (size1 + size2);
    }
    handleEvents();
    takeAnalyses();
    checkEndAt();
    keepArmed();
    engine.setSafetyLimiter (show.safetyLimiter);
    engine.setManualDuck (valueOf (duckParam) >= 0.5f);
    engine.setDuck ({ show.duck.automatic, (float) show.duck.depthDb, (float) show.duck.thresholdDb,
                      (float) show.duck.attackSeconds, (float) show.duck.holdSeconds, (float) show.duck.releaseSeconds });
    engine.setCueLevel (juce::Decibels::decibelsToGain ((float) show.cueLevelDb));

    // Pads: rendered again for a new rate; old sounds let go once the engine has moved on.
    if (padsDirty.exchange (false))
        for (int pad = 0; pad < PlayEngine::kPads; ++pad)
            if (! show.pads[(size_t) pad].isEmpty())
            {
                juce::String problem;
                installPad (pad, renderPad (show.pads[(size_t) pad], problem));
            }
    retiredPads.erase (std::remove_if (retiredPads.begin(), retiredPads.end(),
                                       [this] (const auto& r) { return engine.isHandled (r.first); }),
                       retiredPads.end());

    const auto db = valueOf (levelParam);
    engine.setLevel (db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db));
    if (rig != nullptr)
        rig->update();

    // The Playlist host parameter: another value crossfades into that list.
    const auto wanted = juce::roundToInt (valueOf (playlistParam));
    if (wanted != lastPlaylistParam)
    {
        lastPlaylistParam = wanted;
        if (wanted - 1 < (int) show.playlists.size() && wanted - 1 != show.active)
            switchPlaylist (wanted - 1, true);
    }
}

void RKPlayProcessor::syncPlaylistParam()
{
    const auto v = show.active + 1;
    if (juce::roundToInt (valueOf (playlistParam)) == v)
        return;
    lastPlaylistParam = v;
    playlistParam->setValueNotifyingHost (playlistParam->convertTo0to1 ((float) v));
}

void RKPlayProcessor::pollTriggers()
{
    for (size_t i = 0; i < triggers.size(); ++i)
    {
        auto* p = triggers[i];
        if (p->getValue() < 0.5f)
            continue;
        p->setValueNotifyingHost (0.0f);
        switch (i)
        {
            case 0: play(); break;
            case 1: pause(); break;
            case 2: next(); break;
            case 3: previous(); break;
            case 4: fadeOut(); break;
            case 5: stop(); break;
            default: break;
        }
    }
    for (int pad = 0; pad < PlayEngine::kPads; ++pad)
    {
        auto* p = padParams[(size_t) pad];
        if (p->getValue() < 0.5f)
            continue;
        p->setValueNotifyingHost (0.0f);
        triggerPad (pad);
    }
}

void RKPlayProcessor::handleEvents()
{
    PlayEngine::Event e;
    while (engine.popEvent (e))
    {
        switch (e.type)
        {
            case PlayEngine::Event::Type::advanced:
                if (e.slot == armedSlot)
                {
                    finishCurrent();
                    currentStarted = juce::Time::getMillisecondCounterHiRes() * 0.001;
                    drop (currentSlot);
                    current = armed;
                    currentSlot = armedSlot;
                    armed = {};
                    armedSlot = -1;
                    armedCrossfade = -1.0;
                    commit (current);
                    showChanged();
                }
                break;

            case PlayEngine::Event::Type::underrun:
                if (e.slot == currentSlot)
                {
                    underran = true;
                    setNote ("The disk couldn't keep up with " + current.song.displayName() + ": went on to the next song");
                }
                break;

            case PlayEngine::Event::Type::ended:
                if (e.slot == currentSlot)
                {
                    // The current song ended with nothing armed after it (or ran dry).
                    finishCurrent();
                    drop (currentSlot);
                    const auto wasPlaying = state == State::playing;
                    state = State::stopped;
                    current = {};
                    if (endingAtSongEnd)
                    {
                        endingAtSongEnd = false;
                        setNote ("End at: the music stopped");
                    }
                    else if (wasPlaying && underran)
                    {
                        if (const auto n = peekNext(); n.isValid())
                            startItem (n, 0.0);
                    }
                    underran = false;
                    showChanged();
                }
                break;

            case PlayEngine::Event::Type::cueEnded:
                if (e.slot == cueSlot)
                {
                    drop (cueSlot);
                    cueSong = {};
                    showChanged();
                }
                break;

            case PlayEngine::Event::Type::released:
                break;
        }
    }
}

//==============================================================================
// Playlists

void RKPlayProcessor::playlistEdited (int playlist, const std::function<int (int)>& moveIndex)
{
    // Indices into the list moved: the current and armed songs, its cursor; a new shuffle round.
    auto& p = show.playlists[(size_t) playlist];
    if (current.isValid() && ! current.queued && current.playlist == playlist)
        current.index = moveIndex (current.index);
    if (armed.isValid() && ! armed.queued && armed.playlist == playlist)
        armed.index = moveIndex (armed.index);
    const auto cursor = moveIndex (p.cursor);
    p.cursor = juce::jlimit (0, juce::jmax (0, (int) p.songs.size() - 1), cursor < 0 ? p.cursor : cursor);
    if (cursor < 0)
        setPlayed (playlist, false);
    p.bag.clear();
    p.nextBag.clear();
    p.bagPos = -1;
    attachAnalyses();
    edited();
}

int RKPlayProcessor::addPlaylist (const juce::String& name)
{
    if ((int) show.playlists.size() >= PlayShow::kMaxPlaylists)
        return -1;
    PlayPlaylist p;
    p.name = name.isNotEmpty() ? name : "Playlist " + juce::String ((int) show.playlists.size() + 1);
    show.playlists.push_back (std::move (p));
    cursorPlayed.resize (show.playlists.size(), false);
    edited();
    return (int) show.playlists.size() - 1;
}

void RKPlayProcessor::removePlaylist (int index)
{
    if (show.playlists.size() <= 1 || index < 0 || index >= (int) show.playlists.size())
        return;
    if ((current.isValid() && current.playlist == index && ! current.queued) || (index == show.active && state != State::stopped))
        stop();

    const auto moved = [index] (int i) { return i > index ? i - 1 : i == index ? -1 : i; };
    show.playlists.erase (show.playlists.begin() + index);
    cursorPlayed.erase (cursorPlayed.begin() + index);
    show.active = juce::jlimit (0, (int) show.playlists.size() - 1, moved (show.active) < 0 ? 0 : moved (show.active));
    for (auto& p : show.playlists)
        p.goTo = juce::jmax (0, moved (p.goTo));
    if (current.isValid())
        current.playlist = moved (current.playlist);
    for (auto& q : queue)
        q.playlist = moved (q.playlist);
    if (pendingSwitch >= 0)
        pendingSwitch = moved (pendingSwitch);
    disarm();
    syncPlaylistParam();
    edited();
}

void RKPlayProcessor::renamePlaylist (int index, const juce::String& name)
{
    if (index < 0 || index >= (int) show.playlists.size() || name.trim().isEmpty())
        return;
    show.playlists[(size_t) index].name = name.trim();
    edited();
}

void RKPlayProcessor::setOrder (int index, PlayPlaylist::Order order)
{
    auto& p = show.playlists[(size_t) index];
    p.order = order;
    p.bag.clear();
    p.nextBag.clear();
    p.bagPos = -1;
    edited();
}

void RKPlayProcessor::setAtEnd (int index, PlayPlaylist::AtEnd atEnd, int goTo)
{
    auto& p = show.playlists[(size_t) index];
    p.atEnd = atEnd;
    p.goTo = juce::jlimit (0, (int) show.playlists.size() - 1, goTo);
    p.nextBag.clear();
    edited();
}

void RKPlayProcessor::setStopAfterEach (int index, bool on)
{
    show.playlists[(size_t) index].stopAfterEach = on;
    edited();
}

void RKPlayProcessor::setPlaylistCrossfade (int index, double seconds)
{
    show.playlists[(size_t) index].crossfadeSeconds = juce::jlimit (0.0, PlayShow::kMaxCrossfade, seconds);
    edited();
}

void RKPlayProcessor::setPlaylistGain (int index, double db)
{
    show.playlists[(size_t) index].gainDb = juce::jlimit (-12.0, 12.0, db);
    edited();
}

bool RKPlayProcessor::linkPlaylist (int index, const juce::File& source)
{
    if (index < 0 || index >= (int) show.playlists.size() || ! (source.isDirectory() || PlayShow::isPlaylistFile (source)))
        return false;
    if (current.isValid() && current.playlist == index && ! current.queued && state != State::stopped)
        stop();
    auto& p = show.playlists[(size_t) index];
    p.link = source.isDirectory() ? PlayPlaylist::Link::folder : PlayPlaylist::Link::m3u;
    p.linkFile = source;
    if (p.name.startsWith ("Playlist "))
        p.name = source.getFileNameWithoutExtension();
    show.rescan (index, formats);
    setPlayed (index, false);
    playlistEdited (index, [] (int) { return 0; });
    return true;
}

void RKPlayProcessor::unlinkPlaylist (int index)
{
    auto& p = show.playlists[(size_t) index];
    p.link = PlayPlaylist::Link::none;
    p.linkFile = juce::File();
    edited();
}

void RKPlayProcessor::rescan (int index)
{
    if (index < 0 || index >= (int) show.playlists.size() || ! show.playlists[(size_t) index].isLinked())
        return;
    // The playing song keeps playing; its place is found again by file.
    const auto playingFile = current.isValid() && ! current.queued && current.playlist == index ? current.song.file : juce::File();
    show.rescan (index, formats);
    const auto& songs = show.playlists[(size_t) index].songs;
    playlistEdited (index, [&songs, playingFile] (int i)
    {
        for (int k = 0; k < (int) songs.size(); ++k)
            if (playingFile != juce::File() && songs[(size_t) k].file == playingFile)
                return k;
        return juce::jmin (i, (int) songs.size() - 1);
    });
}

//==============================================================================
// Songs

void RKPlayProcessor::addFiles (int playlist, const juce::Array<juce::File>& files, int index)
{
    if (playlist < 0 || playlist >= (int) show.playlists.size() || show.playlists[(size_t) playlist].isLinked())
        return;

    juce::Array<juce::File> found;
    for (const auto& f : files)
    {
        if (f.isDirectory() || PlayShow::isPlaylistFile (f))
            found.addArray (PlayShow::filesIn (f, true, formats));
        else if (f.existsAsFile())
            found.add (f);
    }
    if (found.isEmpty())
        return;

    auto& songs = show.playlists[(size_t) playlist].songs;
    index = index < 0 || index > (int) songs.size() ? (int) songs.size() : index;
    std::vector<PlaySong> added;
    for (const auto& f : found)
        added.push_back (PlayShow::makeSong (f, formats));
    const auto wasEmpty = songs.empty();
    songs.insert (songs.begin() + index, added.begin(), added.end());

    if (const auto problems = (int) std::count_if (added.begin(), added.end(), [] (const PlaySong& s) { return ! s.isPlayable(); }))
        setNote (juce::String (problems) + (problems == 1 ? " file" : " files") + " can't be played (marked in red)");

    // The songs after the insert point moved down (an empty list just starts at its first song).
    const auto count = (int) added.size();
    playlistEdited (playlist, [wasEmpty, index, count] (int i) { return ! wasEmpty && i >= index ? i + count : i; });
}

void RKPlayProcessor::removeSong (int playlist, int index)
{
    if (playlist < 0 || playlist >= (int) show.playlists.size())
        return;
    auto& p = show.playlists[(size_t) playlist];
    if (p.isLinked() || index < 0 || index >= (int) p.songs.size())
        return;
    if (current.isValid() && ! current.queued && current.playlist == playlist && current.index == index)
        return;   // the playing song stays (the window doesn't offer it)

    p.songs.erase (p.songs.begin() + index);
    playlistEdited (playlist, [index] (int i) { return i > index ? i - 1 : i == index ? -1 : i; });
}

void RKPlayProcessor::moveSong (int playlist, int from, int to)
{
    if (playlist < 0 || playlist >= (int) show.playlists.size())
        return;
    auto& songs = show.playlists[(size_t) playlist].songs;
    const auto n = (int) songs.size();
    if (show.playlists[(size_t) playlist].isLinked() || from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    auto song = std::move (songs[(size_t) from]);
    songs.erase (songs.begin() + from);
    songs.insert (songs.begin() + to, std::move (song));
    playlistEdited (playlist, [from, to] (int i)
    {
        if (i == from) return to;
        if (from < to && i > from && i <= to) return i - 1;
        if (from > to && i >= to && i < from) return i + 1;
        return i;
    });
}

void RKPlayProcessor::clearSongs (int playlist)
{
    if (playlist < 0 || playlist >= (int) show.playlists.size())
        return;
    if (current.isValid() && ! current.queued && current.playlist == playlist && state != State::stopped)
        stop();
    auto& p = show.playlists[(size_t) playlist];
    p.songs.clear();
    p.link = PlayPlaylist::Link::none;
    setPlayed (playlist, false);
    playlistEdited (playlist, [] (int) { return 0; });
}

void RKPlayProcessor::setSongGain (int playlist, int index, double db)
{
    if (playlist < 0 || playlist >= (int) show.playlists.size())
        return;
    auto& songs = show.playlists[(size_t) playlist].songs;
    if (index < 0 || index >= (int) songs.size())
        return;
    songs[(size_t) index].gainDb = juce::jlimit (-12.0, 12.0, db);
    // The copies of it (playing, armed, queued) follow.
    for (auto* item : { &current, &armed })
        if (item->isValid() && item->song.file == songs[(size_t) index].file)
            item->song.gainDb = songs[(size_t) index].gainDb;
    for (auto& q : queue)
        if (q.song.file == songs[(size_t) index].file)
            q.song.gainDb = songs[(size_t) index].gainDb;
    edited();
}

//==============================================================================
// Show-wide settings

void RKPlayProcessor::setFadeOut (double seconds)  { show.fadeOutSeconds = juce::jlimit (0.5, 30.0, seconds); edited(); }
void RKPlayProcessor::setSmartCrossfade (bool on)  { show.smartCrossfade = on; edited(); }
void RKPlayProcessor::setLevelMatch (bool on)      { show.level.match = on; edited(); }
void RKPlayProcessor::setTarget (double lufs)      { show.level.targetLufs = juce::jlimit (-23.0, -14.0, lufs); edited(); }
void RKPlayProcessor::setLift (double db)          { show.level.liftDb = juce::jlimit (0.0, 6.0, db); edited(); }
void RKPlayProcessor::setSafetyLimiter (bool on)   { show.safetyLimiter = on; edited(); }

void RKPlayProcessor::setMusicFolder (const juce::File& folder)
{
    if (const auto found = show.setMusicFolder (folder, formats); found > 0)
    {
        setNote ("Found " + juce::String (found) + (found == 1 ? " song" : " songs") + " in the new Music folder");
        attachAnalyses();
    }
    edited();
}

int RKPlayProcessor::relink (const juce::File& folder)
{
    const auto found = show.relink (folder, formats);
    if (found > 0)
    {
        attachAnalyses();
        edited();
    }
    const auto left = show.numProblems();
    setNote ("Relink: found " + juce::String (found) + (found == 1 ? " song" : " songs")
             + (left > 0 ? ", " + juce::String (left) + " still missing" : juce::String()));
    return found;
}

//==============================================================================
// Analysis: from the cache at once, the rest measured in the background

void RKPlayProcessor::attachAnalyses()
{
    for (auto& p : show.playlists)
        for (auto& song : p.songs)
        {
            if (song.analysis.valid || ! song.isPlayable())
                continue;
            if (cache == nullptr || ! cache->find (song.file, song.analysis))
                if (analyser != nullptr)
                    analyser->add (song.file);
        }
}

void RKPlayProcessor::takeAnalyses()
{
    if (analyser == nullptr)
        return;
    auto results = analyser->takeResults();
    if (results.empty())
        return;
    for (auto& r : results)
    {
        if (! r.analysis.valid)
            continue;
        if (cache != nullptr)
            cache->store (r.file, r.analysis);
        for (auto& p : show.playlists)
            for (auto& song : p.songs)
                if (song.file == r.file)
                    song.analysis = r.analysis;
        // The copies: playing, armed, queued.
        for (auto* item : { &current, &armed })
            if (item->song.file == r.file)
                item->song.analysis = r.analysis;
        for (auto& q : queue)
            if (q.song.file == r.file)
                q.song.analysis = r.analysis;
    }
    // Saved once the queue is through (not after every song).
    if (cache != nullptr && analyser->pending() == 0)
        cache->save();
    showChanged();
}

void RKPlayProcessor::analyseNow()
{
    for (auto& p : show.playlists)
        for (auto& song : p.songs)
        {
            if (song.analysis.valid || ! song.isPlayable())
                continue;
            if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (song.file)); reader != nullptr)
                song.analysis = PlayAnalysis::analyse (*reader);
            if (cache != nullptr && song.analysis.valid)
                cache->store (song.file, song.analysis);
        }
    if (cache != nullptr)
        cache->save();
    showChanged();
}

//==============================================================================
// Pads

std::shared_ptr<PlayEngine::PadSound> RKPlayProcessor::renderPad (const PlayPad& pad, juce::String& problem)
{
    if (sampleRate <= 0.0)
        return {};
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (pad.file));
    if (reader == nullptr || reader->sampleRate <= 0.0)
    {
        problem = "Pad " + pad.name + ": the file is " + (pad.file.existsAsFile() ? "unreadable" : "missing");
        return {};
    }
    const auto length = (int) reader->lengthInSamples;
    if (length / reader->sampleRate > kMaxPadSeconds)
    {
        problem = "Pad " + pad.name + ": the file is longer than a minute (pads hold up to 60 s)";
        return {};
    }

    // Loudness matched like the songs (gain only: a pad never needs lifting).
    auto levelSettings = show.level;
    levelSettings.liftDb = 0.0;
    const auto gain = juce::Decibels::decibelsToGain ((float) PlayLevel::plan (PlayAnalysis::analyse (*reader), levelSettings, 0.0).gainDb);

    // The whole file, then to the device rate (the interpolator's delay dropped).
    constexpr int latency = (int) juce::WindowedSincInterpolator::getBaseLatency();
    juce::AudioBuffer<float> source (2, length + 2 * latency);
    source.clear();
    reader->read (&source, 0, length, 0, true, true);
    auto sound = std::make_shared<PlayEngine::PadSound>();
    const auto ratio = reader->sampleRate / sampleRate;
    if (juce::exactlyEqual (ratio, 1.0))
    {
        sound->audio.setSize (2, length);
        for (int ch = 0; ch < 2; ++ch)
            sound->audio.copyFromWithRamp (ch, 0, source.getReadPointer (ch), length, gain, gain);
        return sound;
    }
    const auto frames = (int) ((length + latency) / ratio);
    const auto lead = (int) std::lround (latency / ratio);
    juce::AudioBuffer<float> resampled (2, frames);
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::WindowedSincInterpolator interpolator;
        interpolator.process (ratio, source.getReadPointer (ch), resampled.getWritePointer (ch), frames);
    }
    sound->audio.setSize (2, juce::jmax (0, frames - lead));
    for (int ch = 0; ch < 2; ++ch)
        sound->audio.copyFromWithRamp (ch, 0, resampled.getReadPointer (ch, lead), sound->audio.getNumSamples(), gain, gain);
    return sound;
}

void RKPlayProcessor::installPad (int pad, std::shared_ptr<PlayEngine::PadSound> sound)
{
    engine.setPad (pad, sound.get());
    // The old sound lives until the engine has handled the command that replaced it.
    if (padSounds[(size_t) pad] != nullptr)
        retiredPads.push_back ({ engine.commandsSent(), padSounds[(size_t) pad] });
    padSounds[(size_t) pad] = std::move (sound);
}

bool RKPlayProcessor::loadPad (int pad, const juce::File& file)
{
    if (pad < 0 || pad >= PlayEngine::kPads)
        return false;
    PlayPad options = show.pads[(size_t) pad];
    if (options.name.isEmpty() || ! options.isEmpty())   // a named, empty pad keeps its name
        options.name = file.getFileNameWithoutExtension();
    options.file = file;
    options.colour = options.colour == 0 ? (pad % 7) + 1 : options.colour;
    juce::String problem;
    auto sound = renderPad (options, problem);
    if (sound == nullptr && sampleRate > 0.0)
    {
        setNote (problem);
        return false;
    }
    show.pads[(size_t) pad] = options;
    installPad (pad, std::move (sound));
    edited();
    return true;
}

void RKPlayProcessor::clearPad (int pad)
{
    if (pad < 0 || pad >= PlayEngine::kPads)
        return;
    show.pads[(size_t) pad] = {};
    installPad (pad, nullptr);
    edited();
}

void RKPlayProcessor::setPadOptions (int pad, const PlayPad& options)
{
    if (pad < 0 || pad >= PlayEngine::kPads)
        return;
    auto& p = show.pads[(size_t) pad];
    const auto file = p.file;
    p = options;
    p.file = file;
    p.levelDb = juce::jlimit (-24.0, 12.0, p.levelDb);
    edited();
}

void RKPlayProcessor::triggerPad (int pad)
{
    if (pad < 0 || pad >= PlayEngine::kPads || padSounds[(size_t) pad] == nullptr)
        return;
    const auto& p = show.pads[(size_t) pad];
    // Stop the music: when it starts (not when a second press stops it).
    if (p.stopsMusic && ! engine.isPadPlaying (pad))
        fadeOut();
    engine.triggerPad (pad, juce::Decibels::decibelsToGain ((float) p.levelDb), p.loop, p.ducksMusic);
}

//==============================================================================
// Ducking and pre-listen

void RKPlayProcessor::setDuck (const PlayShow::Duck& d)
{
    show.duck = d;
    edited();
}

void RKPlayProcessor::setManualDuck (bool on)
{
    duckParam->setValueNotifyingHost (on ? 1.0f : 0.0f);
    engine.setManualDuck (on);
    showChanged();
}

void RKPlayProcessor::preListen (int playlist, int index, double startSeconds)
{
    const auto item = itemAt (playlist, index);
    if (! item.isValid() || ! item.song.isPlayable())
        return;
    const auto slot = load (item, juce::jmax (0.0, startSeconds));
    if (slot < 0)
        return;
    engine.cueStart (slot, 1.0f);
    markSent (slot);
    drop (cueSlot);   // the one it replaces is let go by the engine
    cueSlot = slot;
    cueSong = item.song;
    showChanged();
}

void RKPlayProcessor::stopPreListen()
{
    if (cueSlot < 0)
        return;
    engine.cueStop();
    markSent (cueSlot);
    drop (cueSlot);
    cueSong = {};
    showChanged();
}

void RKPlayProcessor::setCueLevel (double db)
{
    show.cueLevelDb = juce::jlimit (-24.0, 6.0, db);
    edited();
}

bool RKPlayProcessor::hasCueOutput() const
{
    const auto* bus = getBus (false, 1);
    return bus != nullptr && bus->isEnabled();
}

void RKPlayProcessor::finishCurrent()
{
    if (rig == nullptr || ! current.isValid() || currentStarted <= 0.0)
        return;
    const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001 - currentStarted;
    const auto list = current.queued ? juce::String ("Request")
                    : current.playlist >= 0 && current.playlist < (int) show.playlists.size() ? show.playlists[(size_t) current.playlist].name
                                                                                               : juce::String();
    rig->songFinished (list, current.song.title, current.song.artist, current.song.file, seconds);
    currentStarted = 0.0;
}

void RKPlayProcessor::readyToPlay (int playlist, int index)
{
    if (playlist < 0 || playlist >= (int) show.playlists.size())
        return;
    auto& p = show.playlists[(size_t) playlist];
    if (index < 0 || index >= (int) p.songs.size())
        return;
    show.active = playlist;
    p.cursor = index;
    setPlayed (playlist, false);
    syncPlaylistParam();
    showChanged();
}

void RKPlayProcessor::setNote (const juce::String& text)
{
    note = text;
    ++noteVersion;
}

//==============================================================================
// State

juce::var RKPlayProcessor::getExtraState() const
{
    return show.toVar();
}

void RKPlayProcessor::setExtraState (const juce::var& extra)
{
    // A show loading: the music stops (quickly) and the new lists are checked.
    if (state != State::stopped)
        panic();
    queue.clear();
    pendingSwitch = -1;
    endingAtSongEnd = false;
    show = {};
    if (extra.isObject())
        show.fromVar (extra, formats);
    cursorPlayed.assign (show.playlists.size(), false);
    findEndAtDeadline();
    if (const auto problems = show.numProblems())
        setNote (juce::String (problems) + (problems == 1 ? " file" : " files") + " missing or unreadable (marked in red)");
    attachAnalyses();
    stopPreListen();
    for (int pad = 0; pad < PlayEngine::kPads; ++pad)
    {
        juce::String problem;
        installPad (pad, show.pads[(size_t) pad].isEmpty() ? nullptr : renderPad (show.pads[(size_t) pad], problem));
        if (problem.isNotEmpty())
            setNote (problem);
    }
    lastPlaylistParam = juce::roundToInt (valueOf (playlistParam));
    syncPlaylistParam();
    showChanged();
}

juce::AudioProcessorEditor* RKPlayProcessor::createEditor()
{
    return new RKPlayEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKPlayProcessor();
}
