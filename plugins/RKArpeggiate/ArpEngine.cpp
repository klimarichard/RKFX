#include "ArpEngine.h"

#include <algorithm>
#include <cmath>

double ArpEngine::stepStart (long long k, float swing) noexcept
{
    // Steps come in pairs; the second of a pair starts later with swing.
    const auto pair = (double) (k - (k & 1));
    return (k & 1) == 0 ? pair : pair + 2.0 * (double) juce::jlimit (0.5f, 0.75f, swing);
}

void ArpEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    out.ensureSize (4096);
    reset();
}

void ArpEngine::reset()
{
    numHeld = keysDown = numSounding = sequenceLength = 0;
    running = false;
    phase = 0.0;
    nextStep = stepCount = 0;
    lastRandom = -1;
}

void ArpEngine::setSettings (const Settings& s) noexcept
{
    if (s.pattern != settings.pattern || s.octaves != settings.octaves)
        sequenceDirty = true;
    settings = s;
}

void ArpEngine::rebuildSequence() noexcept
{
    sequenceDirty = false;
    std::array<Note, kMaxHeld> base {};
    std::copy (held.begin(), held.begin() + numHeld, base.begin());
    if (settings.pattern != Pattern::played)
        std::sort (base.begin(), base.begin() + numHeld, [] (const Note& a, const Note& b) { return a.note < b.note; });

    // Every octave above, in turn.
    sequenceLength = 0;
    const auto octaves = juce::jlimit (1, 4, settings.octaves);
    for (int o = 0; o < octaves; ++o)
        for (int i = 0; i < numHeld; ++i)
        {
            auto n = base[(size_t) i];
            n.note += 12 * o;
            if (n.note <= 127)
                sequence[(size_t) sequenceLength++] = n;
        }

    if (settings.pattern == Pattern::down)
        std::reverse (sequence.begin(), sequence.begin() + sequenceLength);
    else if (settings.pattern == Pattern::upDown && sequenceLength > 2)
    {
        // Back down without repeating the top and the bottom.
        const auto upLength = sequenceLength;
        for (int i = upLength - 2; i >= 1 && sequenceLength < kMaxSequence; --i)
            sequence[(size_t) sequenceLength++] = sequence[(size_t) i];
    }
}

void ArpEngine::keyDown (const Note& n) noexcept
{
    // Latched: the first key after all were up starts a new set.
    if (settings.latch && keysDown == 0)
        numHeld = 0;
    ++keysDown;

    for (int i = 0; i < numHeld; ++i)
        if (held[(size_t) i].note == n.note && held[(size_t) i].channel == n.channel)
        {
            held[(size_t) i] = n;
            sequenceDirty = true;
            return;
        }

    if (numHeld < kMaxHeld)
        held[(size_t) numHeld++] = n;
    sequenceDirty = true;
}

void ArpEngine::keyUp (int note, int channel) noexcept
{
    keysDown = juce::jmax (0, keysDown - 1);
    if (settings.latch)
        return;

    for (int i = 0; i < numHeld; ++i)
        if (held[(size_t) i].note == note && held[(size_t) i].channel == channel)
        {
            std::move (held.begin() + i + 1, held.begin() + numHeld, held.begin() + i);
            --numHeld;
            sequenceDirty = true;
            return;
        }
}

void ArpEngine::releaseSounding (int position) noexcept
{
    for (int i = 0; i < numSounding; ++i)
        out.addEvent (juce::MidiMessage::noteOff (soundingNotes[(size_t) i].channel, soundingNotes[(size_t) i].note), position);
    numSounding = 0;
}

void ArpEngine::startStep (int position) noexcept
{
    releaseSounding (position);
    if (sequenceDirty)
        rebuildSequence();
    if (sequenceLength == 0)
        return;

    const auto velocityOf = [this] (const Note& n)
    {
        return (juce::uint8) juce::jlimit (1, 127, settings.fixedVelocity ? settings.velocity : n.velocity);
    };

    if (settings.pattern == Pattern::chord)
    {
        // Every held note, one octave further each step.
        const auto octave = (int) (stepCount % juce::jlimit (1, 4, settings.octaves));
        for (int i = 0; i < numHeld; ++i)
        {
            auto n = held[(size_t) i];
            n.note += 12 * octave;
            if (n.note > 127)
                continue;
            out.addEvent (juce::MidiMessage::noteOn (n.channel, n.note, velocityOf (n)), position);
            soundingNotes[(size_t) numSounding++] = n;
        }
    }
    else
    {
        int index;
        if (settings.pattern == Pattern::random)
        {
            index = random.nextInt (sequenceLength);
            if (sequenceLength > 1 && index == lastRandom)
                index = (index + 1 + random.nextInt (sequenceLength - 1)) % sequenceLength;
            lastRandom = index;
        }
        else
        {
            index = (int) (stepCount % sequenceLength);
        }

        const auto& n = sequence[(size_t) index];
        out.addEvent (juce::MidiMessage::noteOn (n.channel, n.note, velocityOf (n)), position);
        soundingNotes[(size_t) numSounding++] = n;
    }

    // The note ends after its share of this step.
    const auto start = stepStart (nextStep, settings.swing);
    const auto length = stepStart (nextStep + 1, settings.swing) - start;
    offAt = start + juce::jlimit (0.05f, 1.0f, settings.gate) * length;
    ++stepCount;
}

void ArpEngine::process (juce::MidiBuffer& midi, int numSamples, const rkfx::Transport& transport) noexcept
{
    out.clear();

    const auto beats = rkfx::SyncDivision::beats (settings.division);
    const auto increment = transport.bpm / 60.0 / sampleRate / beats;   // steps per sample
    const auto synced = transport.playing && transport.hasPosition;

    // Locked to the host: the phase is its position (a jump resyncs).
    if (synced)
    {
        const auto hostPhase = transport.ppq / beats;
        if (! running || std::abs (hostPhase - phase) > 0.5)
        {
            phase = hostPhase;
            nextStep = (long long) std::ceil (phase - 1.0e-9);
            while (stepStart (nextStep, settings.swing) < phase - 1.0e-9)
                ++nextStep;
        }
    }

    auto events = midi.cbegin();
    for (int i = 0; i < numSamples; ++i)
    {
        // This sample's events first.
        for (; events != midi.cend() && (*events).samplePosition <= i; ++events)
        {
            const auto message = (*events).getMessage();
            if (message.isNoteOn())
            {
                const auto wasIdle = numHeld == 0;
                keyDown ({ message.getNoteNumber(), message.getChannel(), message.getVelocity() });
                if (wasIdle && ! running)
                {
                    // A new arpeggio: at once when free, on the next step when locked.
                    running = true;
                    stepCount = 0;
                    lastRandom = -1;
                    if (! synced)
                    {
                        phase = 0.0;
                        nextStep = 0;
                    }
                }
            }
            else if (message.isNoteOff())
            {
                keyUp (message.getNoteNumber(), message.getChannel());
            }
            else
            {
                if (message.isAllNotesOff() || message.isAllSoundOff())
                {
                    releaseSounding (i);
                    numHeld = keysDown = 0;
                    running = false;
                }
                out.addEvent (message, i);
            }
        }

        if (numSounding > 0 && phase >= offAt - 1.0e-9)
            releaseSounding (i);

        if (running)
        {
            if (phase >= stepStart (nextStep, settings.swing) - 1.0e-9)
            {
                if (numHeld > 0)
                    startStep (i);
                else
                    running = false;    // every key up (and not latched): the arpeggio ends
                ++nextStep;
            }
        }

        phase += increment;
    }

    // Events at or after the block's end (shouldn't happen) still go out.
    for (; events != midi.cend(); ++events)
        out.addEvent ((*events).getMessage(), numSamples - 1);

    midi.swapWith (out);
}

void ArpEngine::releaseAll (juce::MidiBuffer& midi) noexcept
{
    for (int i = 0; i < numSounding; ++i)
        midi.addEvent (juce::MidiMessage::noteOff (soundingNotes[(size_t) i].channel, soundingNotes[(size_t) i].note), 0);
    reset();
}
