#include "EchoEngine.h"

#include <cmath>

int EchoEngine::transpose (int note, int steps, const Settings& s)
{
    int result = note + steps;

    if (s.scaleSteps && s.scale != rkfx::scales::Scale::chromatic)
    {
        // Count steps along the scale: the scale note at or below the note, then on.
        const auto notes = rkfx::scales::notesOf (s.scale);
        const auto rel = note - s.key;
        const auto octave = rkfx::scales::floorDiv (rel, 12);
        const auto pitchClass = rel - octave * 12;
        int index = 0;
        for (int i = 0; i < notes.size; ++i)
            if (notes.steps[(size_t) i] <= pitchClass)
                index = i;
        const auto offset = pitchClass - notes.steps[(size_t) index];
        const auto degree = octave * notes.size + index + steps;
        const auto targetOctave = rkfx::scales::floorDiv (degree, notes.size);
        result = s.key + targetOctave * 12 + notes.steps[(size_t) (degree - targetOctave * notes.size)] + offset;
    }

    return result >= 0 && result <= 127 ? result : -1;
}

int EchoEngine::planRepeats (const Settings& s, int note, int velocity, int channel, double echoUnits,
                             std::array<Repeat, kMaxRepeats>& out)
{
    const auto echoChannel = s.channel > 0 ? s.channel : channel;
    auto v = (double) velocity;
    int count = 0;

    for (int k = 1; k <= juce::jlimit (1, kMaxRepeats, s.repeats); ++k)
    {
        v *= juce::jlimit (0.1, 1.0, (double) s.feedback);
        const auto vel = (int) std::lround (v);
        const auto n = transpose (note, k * s.step, s);
        if (vel < juce::jmax (1, s.velocityMin) || n < 0)
            break;

        const auto ch = s.alternate > 0 && k % 2 == 0 ? s.alternate : echoChannel;
        const auto offset = k * echoUnits + (k % 2 == 0 ? juce::jlimit (0.0, 0.5, (double) s.swing) * echoUnits : 0.0);
        out[(size_t) count++] = { offset, n, juce::jmin (127, vel), juce::jlimit (1, 16, ch) };
    }

    if (count < kMaxRepeats)
        out[(size_t) count] = { 0.0, -1, 0, 0 };
    return count;
}

//==============================================================================
void EchoEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    out.ensureSize (8192);
    reset();
}

void EchoEngine::reset()
{
    for (auto& e : events)
        e.used = false;
    numEvents = 0;
    nextDue = 1.0e300;
    for (auto& src : sources)
        src = {};
    nextSource = 0;
    for (auto& channel : owners)
        channel.fill (0);
    for (auto& channel : heldSource)
        channel.fill (-1);
    for (auto& channel : heldOwner)
        channel.fill (0);
    now = 0.0;
}

bool EchoEngine::schedule (const Event& event) noexcept
{
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        for (auto& e : events)
            if (! e.used)
            {
                e = event;
                e.used = true;
                ++numEvents;
                nextDue = juce::jmin (nextDue, event.time);
                return true;
            }
        dropEarliestRepeat();   // full: the earliest pending repeat gives way
    }
    return false;
}

void EchoEngine::dropEarliestRepeat() noexcept
{
    Event* earliest = nullptr;
    for (auto& e : events)
        if (e.used && e.on && (earliest == nullptr || e.time < earliest->time))
            earliest = &e;
    if (earliest != nullptr)
    {
        earliest->used = false;
        --numEvents;
    }
}

void EchoEngine::noteOn (int channel, int note, int velocity, int sample, std::uint32_t& owner) noexcept
{
    auto& current = owners[(size_t) channel - 1][(size_t) note];
    if (current != 0)
        out.addEvent (juce::MidiMessage::noteOff (channel, note), sample);   // the note is taken: end it first

    owner = nextOwner++;
    if (nextOwner == 0)
        nextOwner = 1;
    current = owner;
    out.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)), sample);
}

void EchoEngine::emitDue (int sample, double echoSamples) noexcept
{
    if (now < nextDue)
        return;

    // Note-offs before note-ons at the same moment, so a repeat on the same note isn't cut.
    for (int pass = 0; pass < 2; ++pass)
        for (auto& e : events)
        {
            if (! e.used || e.time > now || e.on != (pass == 1))
                continue;
            e.used = false;
            --numEvents;

            if (! e.on)
            {
                auto& owner = owners[(size_t) e.channel - 1][(size_t) e.note];
                if (owner == e.owner)
                {
                    out.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), sample);
                    owner = 0;
                }
                continue;
            }

            // How long: as played (capped at the echo time), or the gate.
            auto length = settings.asPlayed ? kAsPlayedCap * echoSamples : juce::jlimit (0.1, 1.0, (double) settings.gate) * echoSamples;
            if (settings.asPlayed && e.source >= 0)
            {
                const auto& src = sources[(size_t) e.source];
                if (src.used && src.end >= 0.0)
                    length = juce::jmin (length, src.end - src.start);
            }

            std::uint32_t owner = 0;
            noteOn (e.channel, e.note, e.velocity, sample, owner);
            Event off;
            off.time = std::round (now + juce::jmax (1.0, length));
            off.on = false;
            off.channel = e.channel;
            off.note = e.note;
            off.owner = owner;
            schedule (off);
        }

    nextDue = 1.0e300;
    for (const auto& e : events)
        if (e.used)
            nextDue = juce::jmin (nextDue, e.time);
}

void EchoEngine::process (juce::MidiBuffer& midi, int numSamples, const rkfx::Transport& transport) noexcept
{
    out.clear();

    const auto echoSamples = settings.sync
        ? rkfx::SyncDivision::beats (settings.division) * 60.0 / juce::jmax (1.0, transport.bpm) * sampleRate
        : juce::jmax (1.0f, settings.timeMs) * 0.001 * sampleRate;

    auto input = midi.cbegin();
    for (int i = 0; i < numSamples; ++i)
    {
        for (; input != midi.cend() && (*input).samplePosition <= i; ++input)
        {
            const auto m = (*input).getMessage();
            const auto ch = juce::jlimit (1, 16, m.getChannel());

            if (m.isNoteOn())
            {
                const auto note = m.getNoteNumber(), velocity = (int) m.getVelocity();
                lastNote = note;
                lastVelocity = velocity;

                const auto src = nextSource;
                nextSource = (nextSource + 1) % kMaxSources;
                sources[(size_t) src] = { now, -1.0, true };
                heldSource[(size_t) ch - 1][(size_t) note] = src;

                if (settings.original)
                    noteOn (ch, note, velocity, i, heldOwner[(size_t) ch - 1][(size_t) note]);

                std::array<Repeat, kMaxRepeats> plan;
                const auto count = planRepeats (settings, note, velocity, ch, 1.0, plan);
                for (int k = 0; k < count; ++k)
                {
                    Event e;
                    e.time = std::round (now + plan[(size_t) k].offsetBeats * echoSamples);   // on a sample
                    e.on = true;
                    e.channel = plan[(size_t) k].channel;
                    e.note = plan[(size_t) k].note;
                    e.velocity = plan[(size_t) k].velocity;
                    e.source = src;
                    schedule (e);
                }
            }
            else if (m.isNoteOff())
            {
                const auto note = m.getNoteNumber();
                auto& src = heldSource[(size_t) ch - 1][(size_t) note];
                if (src >= 0)
                {
                    sources[(size_t) src].end = now;
                    src = -1;
                }

                if (settings.original)
                {
                    // Only if the played note still owns it (a repeat may have taken it over).
                    auto& owner = owners[(size_t) ch - 1][(size_t) note];
                    auto& held = heldOwner[(size_t) ch - 1][(size_t) note];
                    if (held != 0 && owner == held)
                    {
                        out.addEvent (m, i);
                        owner = 0;
                    }
                    held = 0;
                }
            }
            else
            {
                out.addEvent (m, i);
            }
        }

        emitDue (i, echoSamples);
        now += 1.0;
    }

    midi.swapWith (out);
}

void EchoEngine::releaseAll (juce::MidiBuffer& midi) noexcept
{
    for (int ch = 0; ch < 16; ++ch)
        for (int note = 0; note < 128; ++note)
            if (owners[(size_t) ch][(size_t) note] != 0)
            {
                midi.addEvent (juce::MidiMessage::noteOff (ch + 1, note), 0);
                owners[(size_t) ch][(size_t) note] = 0;
            }

    for (auto& e : events)
        e.used = false;
    numEvents = 0;
    nextDue = 1.0e300;
    for (auto& channel : heldOwner)
        channel.fill (0);
}
