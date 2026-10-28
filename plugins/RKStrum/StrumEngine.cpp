#include "StrumEngine.h"

#include <algorithm>

void StrumEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    out.ensureSize (4096);
    reset();
}

void StrumEngine::reset()
{
    now = 0;
    chordSize = queueSize = 0;
    windowEnd = -1;
    nextUp = false;
    for (auto& channel : startsAt)
        channel.fill (-1);
    for (auto& channel : sounding)
        channel.fill (false);
}

void StrumEngine::schedule (long long time, const juce::MidiMessage& message) noexcept
{
    if (queueSize >= kMaxQueue)
        return;

    // Kept in time order (stable: equal times stay in the order given).
    auto at = queueSize;
    while (at > 0 && queue[(size_t) at - 1].time > time)
    {
        queue[(size_t) at] = queue[(size_t) at - 1];
        --at;
    }
    queue[(size_t) at] = { time, message };
    ++queueSize;
}

void StrumEngine::strum() noexcept
{
    auto up = settings.direction == Direction::up;
    if (settings.direction == Direction::alternate)
    {
        up = nextUp;
        nextUp = ! nextUp;
    }

    std::sort (chord.begin(), chord.begin() + chordSize,
               [up] (const Pending& a, const Pending& b) { return up ? a.note > b.note : a.note < b.note; });

    const auto spacing = settings.speedMs * 0.001 * sampleRate;
    for (int i = 0; i < chordSize; ++i)
    {
        const auto& n = chord[(size_t) i];
        auto offset = i * spacing;
        if (i > 0 && settings.humanize > 0.0f)
            offset += (random.nextDouble() - 0.5) * settings.humanize * spacing;

        // The ramp: from the first note to the last.
        const auto position = chordSize > 1 ? (double) i / (chordSize - 1) : 0.0;
        const auto velocity = juce::jlimit (1, 127, juce::roundToInt (n.velocity * (1.0 + settings.ramp * position)));

        const auto time = now + (long long) juce::jmax (0.0, offset);
        schedule (time, juce::MidiMessage::noteOn (n.channel, n.note, (juce::uint8) velocity));
        startsAt[(size_t) n.channel - 1][(size_t) n.note] = time;

        // Released already (a quick chord): it still sounds briefly.
        if (n.released)
            schedule (time + (long long) (kMinLengthMs * 0.001 * sampleRate), juce::MidiMessage::noteOff (n.channel, n.note));
    }

    chordSize = 0;
    windowEnd = -1;
}

void StrumEngine::emitDue (int position) noexcept
{
    auto done = 0;
    while (done < queueSize && queue[(size_t) done].time <= now)
    {
        const auto& m = queue[(size_t) done].message;
        out.addEvent (m, position);
        const auto channel = juce::jlimit (0, 15, m.getChannel() - 1);
        if (m.isNoteOn())
        {
            sounding[(size_t) channel][(size_t) m.getNoteNumber()] = true;
            startsAt[(size_t) channel][(size_t) m.getNoteNumber()] = -1;
        }
        else if (m.isNoteOff())
        {
            sounding[(size_t) channel][(size_t) m.getNoteNumber()] = false;
        }
        ++done;
    }

    if (done > 0)
    {
        std::move (queue.begin() + done, queue.begin() + queueSize, queue.begin());
        queueSize -= done;
    }
}

void StrumEngine::process (juce::MidiBuffer& midi, int numSamples) noexcept
{
    out.clear();
    const auto window = (long long) (settings.windowMs * 0.001 * sampleRate);
    const auto minLength = (long long) (kMinLengthMs * 0.001 * sampleRate);

    auto events = midi.cbegin();
    for (int i = 0; i < numSamples; ++i, ++now)
    {
        for (; events != midi.cend() && (*events).samplePosition <= i; ++events)
        {
            const auto message = (*events).getMessage();
            const auto channel = message.getChannel();

            if (message.isNoteOn())
            {
                // Joins the chord being gathered (or starts one).
                if (chordSize == 0)
                    windowEnd = now + window;
                if (chordSize < kMaxChord)
                    chord[(size_t) chordSize++] = { message.getNoteNumber(), channel, message.getVelocity(), false };
            }
            else if (message.isNoteOff())
            {
                const auto note = message.getNoteNumber();
                auto gathered = false;
                for (int c = 0; c < chordSize; ++c)
                    if (chord[(size_t) c].note == note && chord[(size_t) c].channel == channel)
                    {
                        chord[(size_t) c].released = true;   // ends shortly after the strum plays it
                        gathered = true;
                    }

                // Queued but not sounded yet: it ends shortly after it starts.
                const auto start = startsAt[(size_t) channel - 1][(size_t) note];
                if (gathered)
                    continue;
                if (start >= 0)
                    schedule (start + minLength, message);
                else
                    out.addEvent (message, i);
            }
            else
            {
                out.addEvent (message, i);
            }
        }

        if (chordSize > 0 && now >= windowEnd)
            strum();
        emitDue (i);
    }

    // Events at or after the block's end (shouldn't happen) still go out.
    for (; events != midi.cend(); ++events)
        out.addEvent ((*events).getMessage(), juce::jmax (0, numSamples - 1));

    midi.swapWith (out);
}

void StrumEngine::releaseAll (juce::MidiBuffer& midi) noexcept
{
    for (int channel = 0; channel < 16; ++channel)
        for (int note = 0; note < 128; ++note)
            if (sounding[(size_t) channel][(size_t) note])
                midi.addEvent (juce::MidiMessage::noteOff (channel + 1, note), 0);
    reset();
}
