#include "ConductEngine.h"

#include <algorithm>

std::unique_ptr<ConductEngine::Song> ConductEngine::makeSong (const juce::MemoryBlock& data)
{
    juce::MemoryInputStream stream (data, false);
    juce::MidiFile file;
    if (! file.readFrom (stream))
        return nullptr;
    return makeSong (file);
}

std::unique_ptr<ConductEngine::Song> ConductEngine::makeSong (const juce::MidiFile& file)
{
    const auto ticksPerQuarter = (double) file.getTimeFormat();
    if (ticksPerQuarter <= 0.0)
        return nullptr;   // SMPTE time: not in beats

    auto s = std::make_unique<Song>();
    auto tempoFound = false;

    for (int t = 0; t < file.getNumTracks() && (int) s->tracks.size() < kMaxTracks; ++t)
    {
        const auto* sequence = file.getTrack (t);
        TrackInfo info;
        const auto track = (int) s->tracks.size();

        for (int i = 0; i < sequence->getNumEvents(); ++i)
        {
            const auto& m = sequence->getEventPointer (i)->message;
            const auto beat = m.getTimeStamp() / ticksPerQuarter;

            if (m.isTrackNameEvent() && info.name.isEmpty())
                info.name = m.getTextFromTextMetaEvent();
            if (m.isTempoMetaEvent() && ! tempoFound)
            {
                s->tempoBpm = 60.0 / m.getTempoSecondsPerQuarterNote();
                tempoFound = true;
            }
            if (m.isEndOfTrackMetaEvent())
                s->lengthBeats = juce::jmax (s->lengthBeats, beat);
            if (m.isMetaEvent())
                continue;

            if (m.isNoteOn())
                ++info.notes;
            s->events.push_back ({ beat, track, m });
            s->lengthBeats = juce::jmax (s->lengthBeats, beat);
        }

        // Tracks with only meta events (a tempo track) are left out of the list.
        if (info.notes > 0 || (! s->events.empty() && s->events.back().track == track))
        {
            if (info.name.isEmpty())
                info.name = "Track " + juce::String (t + 1);
            s->tracks.push_back (info);
        }
    }

    // In time; at the same time, note-offs first (a repeated note restarts cleanly).
    std::stable_sort (s->events.begin(), s->events.end(), [] (const Song::Event& a, const Song::Event& b)
    {
        if (! juce::exactlyEqual (a.beat, b.beat))
            return a.beat < b.beat;
        return a.message.isNoteOff() && ! b.message.isNoteOff();
    });
    return s;
}

ConductEngine::ConductEngine()
{
    settings.trackOn.fill (true);
    settings.trackChannel.fill (0);
}

void ConductEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    out.ensureSize (8192);
    const juce::SpinLock::ScopedLockType sl (songLock);
    stopNow (0);
    out.clear();
}

std::unique_ptr<ConductEngine::Song> ConductEngine::setSong (std::unique_ptr<Song> next)
{
    const juce::SpinLock::ScopedLockType sl (songLock);
    stopRequested = true;      // the notes of the old song end in the next block
    std::swap (song, next);
    return next;
}

void ConductEngine::endAll (int sample) noexcept
{
    for (auto& track : soundingChannel)
        for (int note = 0; note < 128; ++note)
            if (track[(size_t) note] != 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (track[(size_t) note], note), sample);
                track[(size_t) note] = 0;
            }
}

void ConductEngine::stopNow (int sample) noexcept
{
    endAll (sample);
    playing = false;
    waiting = false;
    position = 0.0;
    cursor = 0;
    countedTaps = 0;
    lastTap = -1;
    lastInterval = 0.0;
    tempo = settings.tempoBpm;
    tapsToStart = settings.countIn + 1;
}

void ConductEngine::advanceTo (double beat, int sample, bool skipping) noexcept
{
    if (song == nullptr)
        return;

    auto& events = song->events;
    while (cursor < events.size() && events[cursor].beat < beat)
    {
        const auto& e = events[cursor++];
        const auto& m = e.message;
        const auto track = (size_t) e.track;

        if (m.isNoteOn())
        {
            // Skipped (an early tap), or a track switched off: not played.
            if (skipping || ! settings.trackOn[track])
                continue;
            const auto channel = settings.trackChannel[track] > 0 ? settings.trackChannel[track] : m.getChannel();
            auto& sounding = soundingChannel[track][(size_t) m.getNoteNumber()];
            if (sounding != 0)
                out.addEvent (juce::MidiMessage::noteOff (sounding, m.getNoteNumber()), sample);
            out.addEvent (juce::MidiMessage::noteOn (channel, m.getNoteNumber(), m.getVelocity()), sample);
            sounding = (uint8_t) channel;
        }
        else if (m.isNoteOff())
        {
            // Wherever its note-on went.
            auto& sounding = soundingChannel[track][(size_t) m.getNoteNumber()];
            if (sounding != 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (sounding, m.getNoteNumber()), sample);
                sounding = 0;
            }
        }
        else if (settings.trackOn[track])
        {
            // Controllers, pitch bend, programs: always (also when skipping, they are state).
            auto copy = m;
            if (settings.trackChannel[track] > 0 && copy.getChannel() > 0)
                copy.setChannel (settings.trackChannel[track]);
            out.addEvent (copy, sample);
        }
    }
}

void ConductEngine::onTap (int sample) noexcept
{
    // A double trigger: sooner than a third of a beat after the last tap.
    const auto expected = settings.tapBeats * 60.0 / tempo * sampleRate;
    if (lastTap >= 0 && (double) (now - lastTap) < juce::jmax (0.04 * sampleRate, expected / 3.0))
        return;

    const auto interval = lastTap >= 0 ? (double) (now - lastTap) / sampleRate : 0.0;
    lastTap = now;

    // The tempo: from the last two intervals (one, after a big change).
    if (interval > 0.0)
    {
        auto seconds = interval;
        if (lastInterval > 0.0 && interval / lastInterval > 0.7 && interval / lastInterval < 1.4)
            seconds = 0.5 * (interval + lastInterval);
        lastInterval = interval;
        tempo = juce::jlimit (20.0, 400.0, 60.0 * settings.tapBeats / seconds);
    }

    if (! playing)
    {
        // Count-in taps, then the start.
        if (++countedTaps <= settings.countIn)
        {
            tapsToStart = settings.countIn + 1 - countedTaps;
            return;
        }
        if (interval <= 0.0)
            tempo = settings.tempoBpm;
        playing = true;
        tapsToStart = 0;
        position = 0.0;
        cursor = 0;
        nextTapPoint = settings.tapBeats;
        return;
    }

    // Early: skip to the tap point. Late (waiting there): go on.
    if (position < nextTapPoint)
    {
        advanceTo (nextTapPoint, sample, true);
        position = nextTapPoint;
    }
    nextTapPoint += settings.tapBeats;
}

void ConductEngine::process (juce::MidiBuffer& midi, int numSamples) noexcept
{
    out.clear();
    const juce::SpinLock::ScopedLockType sl (songLock);

    if (stopRequested.exchange (false))
        stopNow (0);

    // Taps from the host parameter, at the start of the block.
    for (auto n = pendingTaps.exchange (0); n > 0; --n)
        onTap (0);

    auto events = midi.cbegin();
    for (int i = 0; i < numSamples; ++i, ++now)
    {
        for (; events != midi.cend() && (*events).samplePosition <= i; ++events)
        {
            const auto message = (*events).getMessage();
            if ((message.isNoteOn() || message.isNoteOff()) && isTapKey (message.getNoteNumber()))
            {
                if (message.isNoteOn())
                    onTap (i);   // the tap keys are taken in
            }
            else if (message.isController() && message.getControllerNumber() == settings.tapController)
            {
                const auto down = message.getControllerValue() >= 64;
                if (down && ! ccDown)
                    onTap (i);
                ccDown = down;
            }
            else
            {
                out.addEvent (message, i);   // the rest of the keyboard plays on
            }
        }

        if (playing && song != nullptr)
        {
            // Up to the next tap point at the measured tempo, never past it.
            const auto next = juce::jmin (position + tempo / 60.0 / sampleRate, nextTapPoint);
            waiting = next >= nextTapPoint;
            advanceTo (next, i, false);
            position = next;

            // The end: loop, or stop.
            if (song->lengthBeats > 0.0 && position >= song->lengthBeats)
            {
                if (settings.loop)
                {
                    advanceTo (song->lengthBeats + 1.0e-9, i, false);
                    position -= song->lengthBeats;
                    nextTapPoint -= song->lengthBeats;
                    cursor = 0;
                    advanceTo (position, i, false);
                }
                else
                {
                    advanceTo (song->lengthBeats + 1.0e-9, i, false);
                    stopNow (i);
                }
            }
        }
    }

    for (; events != midi.cend(); ++events)
        out.addEvent ((*events).getMessage(), juce::jmax (0, numSamples - 1));

    shownPosition = position;
    shownTempo = tempo;
    midi.swapWith (out);
}

void ConductEngine::releaseAll (juce::MidiBuffer& midi) noexcept
{
    const juce::SpinLock::ScopedLockType sl (songLock);
    out.clear();
    stopNow (0);
    for (const auto metadata : out)
        midi.addEvent (metadata.getMessage(), 0);
}
