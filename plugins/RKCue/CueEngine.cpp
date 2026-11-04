#include "CueEngine.h"

CueEngine::Settings CueEngine::defaultSettings()
{
    Settings s;
    for (int n = 0; n < kSlots; ++n)
    {
        auto& slot = s.slots[(size_t) n];
        slot.low = slot.high = slot.root = 36 + n;
    }
    return s;
}

CueEngine::CueEngine()
{
    settings = defaultSettings();
    for (auto& p : progress)
        p = 0.0f;
}

void CueEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    buffer.ensureSize (16384);
    const juce::SpinLock::ScopedLockType sl (songLock);
    for (auto& v : voices)
        endVoice (v, 0);
    buffer.clear();
    takenIn.fill (false);
    passed.fill (false);
    outCount.fill (0);
}

std::unique_ptr<CueEngine::Song> CueEngine::setSong (int slot, std::unique_ptr<Song> next)
{
    const juce::SpinLock::ScopedLockType sl (songLock);
    stopRequests |= bit (slot);    // the old file's notes end in the next block
    std::swap (songs[(size_t) slot], next);
    return next;
}

//==============================================================================
bool CueEngine::inZone (int slot, int key) const noexcept
{
    if (songs[(size_t) slot] == nullptr)
        return false;
    const auto& s = settings.slots[(size_t) slot];
    return key >= juce::jmin (s.low, s.high) && key <= juce::jmax (s.low, s.high);
}

void CueEngine::setKey (int note, bool down) noexcept
{
    auto& word = note < 64 ? keysLow : keysHigh;
    const auto mask = (uint64_t) 1 << (note & 63);
    if (down)
        word.fetch_or (mask);
    else
        word.fetch_and (~mask);
}

void CueEngine::noteOn (Voice& v, int inChannel, int inNote, int outChannel, int outNote, int velocity, int sample) noexcept
{
    auto& slot = v.out[(size_t) ((inChannel - 1) * 128 + inNote)];
    if (slot != 0)
    {
        // The file repeats a note that is still sounding: end it first.
        noteOff (slot, sample);
        --v.sounding;
    }
    buffer.addEvent (juce::MidiMessage::noteOn (outChannel, outNote, (juce::uint8) velocity), sample);
    auto& count = outCount[(size_t) ((outChannel - 1) * 128 + outNote)];
    if (count < 255)
        ++count;
    slot = (uint16_t) (outChannel << 8 | outNote);
    ++v.sounding;
}

void CueEngine::noteOff (uint16_t packed, int sample) noexcept
{
    const auto channel = packed >> 8, note = packed & 0x7f;
    auto& count = outCount[(size_t) ((channel - 1) * 128 + note)];
    if (count > 0)
        --count;
    // Another copy still sounds it: its own note-off ends it.
    if (count == 0)
        buffer.addEvent (juce::MidiMessage::noteOff (channel, note), sample);
}

void CueEngine::endNotes (Voice& v, int sample) noexcept
{
    if (v.sounding > 0)
        for (auto& packed : v.out)
            if (packed != 0)
            {
                noteOff (packed, sample);
                packed = 0;
            }
    v.sounding = 0;
}

void CueEngine::endVoice (Voice& v, int sample) noexcept
{
    endNotes (v, sample);
    v.active = false;
}

void CueEngine::stopSlot (int slot, int sample) noexcept
{
    for (auto& v : voices)
        if (v.active && v.slot == slot)
            endVoice (v, sample);
}

//==============================================================================
void CueEngine::trigger (int slotIndex, int key, int velocity, int sample) noexcept
{
    const auto& s = settings.slots[(size_t) slotIndex];

    if (s.mode == Mode::toggle)
    {
        // Pressed again while playing: stop.
        auto stopped = false;
        for (auto& v : voices)
            if (v.active && v.slot == slotIndex && v.key == key)
            {
                endVoice (v, sample);
                stopped = true;
            }
        if (stopped)
            return;
    }

    if (! s.poly)
        stopSlot (slotIndex, sample);   // restart
    else
    {
        Voice* oldest = nullptr;
        int count = 0;
        for (auto& v : voices)
            if (v.active && v.slot == slotIndex)
            {
                ++count;
                if (oldest == nullptr || v.order < oldest->order)
                    oldest = &v;
            }
        if (count >= kPolyPerSlot && oldest != nullptr)
            endVoice (*oldest, sample);
    }

    Voice* voice = nullptr;
    for (auto& v : voices)
        if (! v.active)
        {
            voice = &v;
            break;
        }
    if (voice == nullptr)
    {
        // All 32 are playing: the oldest one makes way.
        voice = &voices[0];
        for (auto& v : voices)
            if (v.order < voice->order)
                voice = &v;
        endVoice (*voice, sample);
    }

    auto& v = *voice;
    v.active = true;
    v.slot = slotIndex;
    v.key = key;
    v.transpose = s.transpose + (s.trackPitch && key != kButton ? key - s.root : 0);
    v.velocityScale = 1.0f - s.velocity + s.velocity * (float) velocity / 127.0f;
    v.position = 0.0;
    v.cursor = 0;
    v.tempoIndex = 0;
    v.order = ++nextOrder;
    v.sounding = 0;
}

void CueEngine::releaseKey (int slotIndex, int key, int sample) noexcept
{
    if (settings.slots[(size_t) slotIndex].mode != Mode::gate)
        return;
    for (auto& v : voices)
        if (v.active && v.slot == slotIndex && v.key == key)
            endVoice (v, sample);
}

void CueEngine::handleIncoming (const juce::MidiMessage& m, int sample) noexcept
{
    if (m.isNoteOn() || m.isNoteOff())
    {
        const auto channel = m.getChannel(), key = m.getNoteNumber();
        const auto index = (size_t) ((channel - 1) * 128 + key);

        if (m.isNoteOn())
        {
            setKey (key, true);
            lastNote = key;
            ++noteCount;

            auto taken = false;
            if (settings.triggerChannel == 0 || channel == settings.triggerChannel)
                for (int slot = 0; slot < kSlots; ++slot)
                    if (inZone (slot, key))
                    {
                        trigger (slot, key, m.getVelocity(), sample);
                        taken = true;
                    }

            if (taken)
            {
                if (passed[index])
                {
                    // It was passed on before it became a trigger key: end it there.
                    buffer.addEvent (juce::MidiMessage::noteOff (channel, key), sample);
                    passed[index] = false;
                }
                takenIn[index] = true;
            }
            else if (settings.thru)
            {
                buffer.addEvent (m, sample);
                passed[index] = true;
            }
            return;
        }

        setKey (key, false);
        const auto wasTaken = takenIn[index];
        if (wasTaken)
        {
            takenIn[index] = false;
            for (int slot = 0; slot < kSlots; ++slot)
                releaseKey (slot, key, sample);
        }
        // A note-off we don't know (played while bypassed) passes on too.
        if (passed[index] || (settings.thru && ! wasTaken))
            buffer.addEvent (m, sample);
        passed[index] = false;
        return;
    }

    if (settings.thru)
        buffer.addEvent (m, sample);
}

//==============================================================================
double CueEngine::tempoOf (Voice& v, const Song& song) noexcept
{
    switch (settings.tempoSource)
    {
        case TempoSource::fixed:
            return juce::jlimit (5.0, 1000.0, settings.fixedBpm);
        case TempoSource::host:
            if (settings.hostHasBpm)
                return juce::jlimit (5.0, 1000.0, settings.hostBpm);
            break;
        case TempoSource::file:
            break;
    }

    const auto& map = song.tempoMap;
    while (v.tempoIndex + 1 < map.size() && map[v.tempoIndex + 1].beat <= v.position)
        ++v.tempoIndex;
    return map[juce::jmin (v.tempoIndex, map.size() - 1)].bpm;
}

void CueEngine::advance (Voice& v, double toBeat, int sample) noexcept
{
    const auto& song = *songs[(size_t) v.slot];
    const auto& s = settings.slots[(size_t) v.slot];
    const auto& events = song.events;

    while (v.cursor < events.size() && events[v.cursor].beat < toBeat)
    {
        const auto& m = events[v.cursor++].message;
        const auto channel = m.getChannel();

        if (m.isNoteOn())
        {
            const auto shift = s.keepDrums && channel == 10 ? 0 : v.transpose;
            const auto note = m.getNoteNumber() + shift;
            if (note < 0 || note > 127)
                continue;   // transposed off the keyboard
            const auto outChannel = s.channel > 0 ? s.channel : channel;
            const auto velocity = juce::jlimit (1, 127, juce::roundToInt ((float) m.getVelocity() * v.velocityScale));
            noteOn (v, channel, m.getNoteNumber(), outChannel, note, velocity, sample);
        }
        else if (m.isNoteOff())
        {
            // Wherever its note-on went.
            auto& packed = v.out[(size_t) ((channel - 1) * 128 + m.getNoteNumber())];
            if (packed != 0)
            {
                noteOff (packed, sample);
                packed = 0;
                --v.sounding;
            }
        }
        else if (m.isAftertouch())
        {
            // Polyphonic pressure: follows its note.
            const auto packed = v.out[(size_t) ((channel - 1) * 128 + m.getNoteNumber())];
            if (packed != 0)
                buffer.addEvent (juce::MidiMessage::aftertouchChange (packed >> 8, packed & 0x7f, m.getAfterTouchValue()), sample);
        }
        else
        {
            // Controllers, pitch bend, programs, pressure, sysex.
            auto copy = m;
            if (s.channel > 0 && copy.getChannel() > 0)
                copy.setChannel (s.channel);
            buffer.addEvent (copy, sample);
        }
    }
}

void CueEngine::step (Voice& v, int sample) noexcept
{
    const auto* song = songs[(size_t) v.slot].get();
    if (song == nullptr)
    {
        endVoice (v, sample);
        return;
    }

    const auto next = v.position + tempoOf (v, *song) / 60.0 / sampleRate;
    advance (v, next, sample);
    v.position = next;

    if (v.position < song->lengthBeats)
        return;

    // The end: everything up to it, then loop, or stop.
    advance (v, song->lengthBeats + 1.0e-9, sample);
    if (settings.slots[(size_t) v.slot].loop && song->lengthBeats > 0.0)
    {
        endNotes (v, sample);
        v.position -= song->lengthBeats;
        v.cursor = 0;
        v.tempoIndex = 0;
        advance (v, v.position, sample);
    }
    else
    {
        endVoice (v, sample);
    }
}

void CueEngine::process (juce::MidiBuffer& midi, int numSamples) noexcept
{
    buffer.clear();
    const juce::SpinLock::ScopedLockType sl (songLock);

    // The host buttons, at the start of the block: stop, play, release.
    const auto stops = stopRequests.exchange (0), plays = playRequests.exchange (0), releases = releaseRequests.exchange (0);
    for (int slot = 0; slot < kSlots; ++slot)
    {
        if (stops & bit (slot))
            stopSlot (slot, 0);
        if ((plays & bit (slot)) && songs[(size_t) slot] != nullptr)
            trigger (slot, kButton, 100, 0);
        if (releases & bit (slot))
            releaseKey (slot, kButton, 0);
    }

    auto events = midi.cbegin();
    for (int i = 0; i < numSamples; ++i)
    {
        for (; events != midi.cend() && (*events).samplePosition <= i; ++events)
            handleIncoming ((*events).getMessage(), i);

        for (auto& v : voices)
            if (v.active)
                step (v, i);
    }
    for (; events != midi.cend(); ++events)
        handleIncoming ((*events).getMessage(), juce::jmax (0, numSamples - 1));

    // For the display.
    for (int slot = 0; slot < kSlots; ++slot)
    {
        int count = 0;
        const Voice* newest = nullptr;
        for (const auto& v : voices)
            if (v.active && v.slot == slot)
            {
                ++count;
                if (newest == nullptr || v.order > newest->order)
                    newest = &v;
            }
        copies[(size_t) slot] = count;
        if (newest != nullptr && songs[(size_t) slot] != nullptr && songs[(size_t) slot]->lengthBeats > 0.0)
            progress[(size_t) slot] = (float) juce::jlimit (0.0, 1.0, newest->position / songs[(size_t) slot]->lengthBeats);
        else
            progress[(size_t) slot] = 0.0f;
    }

    midi.swapWith (buffer);
}

void CueEngine::releaseAll (juce::MidiBuffer& midi) noexcept
{
    const juce::SpinLock::ScopedLockType sl (songLock);
    buffer.clear();
    for (auto& v : voices)
        endVoice (v, 0);
    takenIn.fill (false);
    passed.fill (false);
    for (const auto metadata : buffer)
        midi.addEvent (metadata.getMessage(), 0);
    buffer.clear();
    for (auto& c : copies)
        c = 0;
}
