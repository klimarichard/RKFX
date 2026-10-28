#include "HarmonizeEngine.h"

namespace
{
    using Scale = HarmonizeEngine::Scale;
    using Interval = HarmonizeEngine::Interval;

    struct ScaleNotes { std::array<int, 7> steps; int size; };

    ScaleNotes notesOf (Scale s) noexcept
    {
        switch (s)
        {
            case Scale::major:           return { { 0, 2, 4, 5, 7, 9, 11 }, 7 };
            case Scale::naturalMinor:    return { { 0, 2, 3, 5, 7, 8, 10 }, 7 };
            case Scale::harmonicMinor:   return { { 0, 2, 3, 5, 7, 8, 11 }, 7 };
            case Scale::melodicMinor:    return { { 0, 2, 3, 5, 7, 9, 11 }, 7 };
            case Scale::dorian:          return { { 0, 2, 3, 5, 7, 9, 10 }, 7 };
            case Scale::phrygian:        return { { 0, 1, 3, 5, 7, 8, 10 }, 7 };
            case Scale::lydian:          return { { 0, 2, 4, 6, 7, 9, 11 }, 7 };
            case Scale::mixolydian:      return { { 0, 2, 4, 5, 7, 9, 10 }, 7 };
            case Scale::majorPentatonic: return { { 0, 2, 4, 7, 9 }, 5 };
            case Scale::minorPentatonic: return { { 0, 3, 5, 7, 10 }, 5 };
            case Scale::blues:           return { { 0, 3, 5, 6, 7, 10 }, 6 };
            case Scale::chromatic:       break;
        }
        return { {}, 0 };
    }

    /** Scale steps (seven-note scales), or 0 for octaves (handled apart). */
    int stepsOf (Interval i) noexcept
    {
        switch (i)
        {
            case Interval::sixthBelow:  return -5;
            case Interval::fifthBelow:  return -4;
            case Interval::fourthBelow: return -3;
            case Interval::thirdBelow:  return -2;
            case Interval::thirdAbove:  return 2;
            case Interval::fourthAbove: return 3;
            case Interval::fifthAbove:  return 4;
            case Interval::sixthAbove:  return 5;
            case Interval::off:
            case Interval::octaveBelow:
            case Interval::octaveAbove: break;
        }
        return 0;
    }

    /** The interval as in a major scale (chromatic, and the nearest-note scales). */
    int semitonesOf (Interval i) noexcept
    {
        switch (i)
        {
            case Interval::octaveBelow: return -12;
            case Interval::sixthBelow:  return -9;
            case Interval::fifthBelow:  return -7;
            case Interval::fourthBelow: return -5;
            case Interval::thirdBelow:  return -4;
            case Interval::thirdAbove:  return 4;
            case Interval::fourthAbove: return 5;
            case Interval::fifthAbove:  return 7;
            case Interval::sixthAbove:  return 9;
            case Interval::octaveAbove: return 12;
            case Interval::off:         break;
        }
        return 0;
    }

    int floorDiv (int a, int b) noexcept { return a >= 0 ? a / b : -((-a + b - 1) / b); }
}

int HarmonizeEngine::harmonyNote (int note, int key, Scale scale, Interval interval) noexcept
{
    if (interval == Interval::off)
        return -1;

    int result = note;
    if (interval == Interval::octaveBelow || interval == Interval::octaveAbove)
    {
        result = note + semitonesOf (interval);
    }
    else if (scale == Scale::chromatic)
    {
        result = note + semitonesOf (interval);
    }
    else
    {
        const auto notes = notesOf (scale);
        const auto rel = note - key;
        const auto octave = floorDiv (rel, 12);
        const auto pitchClass = rel - octave * 12;

        // The scale note at or below the played note.
        int index = 0;
        for (int i = 0; i < notes.size; ++i)
            if (notes.steps[(size_t) i] <= pitchClass)
                index = i;
        const auto offset = pitchClass - notes.steps[(size_t) index];

        if (notes.size == 7)
        {
            // Diatonic: move by scale steps.
            const auto degree = octave * 7 + index + stepsOf (interval);
            const auto targetOctave = floorDiv (degree, 7);
            const auto targetIndex = degree - targetOctave * 7;
            result = key + targetOctave * 12 + notes.steps[(size_t) targetIndex] + offset;
        }
        else
        {
            // Five- or six-note scales: the major-scale interval, then the nearest scale note.
            const auto wanted = note - offset + semitonesOf (interval);
            auto best = wanted, bestDistance = 99;
            for (int o = -1; o <= 1; ++o)
                for (int i = 0; i < notes.size; ++i)
                {
                    const auto candidate = key + (floorDiv (wanted - key, 12) + o) * 12 + notes.steps[(size_t) i];
                    if (std::abs (candidate - wanted) < bestDistance)
                    {
                        bestDistance = std::abs (candidate - wanted);
                        best = candidate;
                    }
                }
            result = best + offset;
        }
    }

    return result >= 0 && result <= 127 ? result : -1;
}

void HarmonizeEngine::prepare()
{
    out.ensureSize (4096);
    reset();
}

void HarmonizeEngine::reset()
{
    for (auto& channel : sounding)
        channel.fill (0);
    for (auto& channel : started)
        for (auto& note : channel)
            note.fill (-1);
}

void HarmonizeEngine::noteOn (int channel, int note, juce::uint8 velocity, int position) noexcept
{
    auto& voices = started[(size_t) channel][(size_t) note];
    if (voices[0] >= 0 || voices[1] >= 0 || voices[2] >= 0)
        noteOff (channel, note, position);   // the same key again without a release

    int pitches[kVoices] = {
        settings.original ? note : -1,
        harmonyNote (note, settings.key, settings.scale, settings.voice1),
        harmonyNote (note, settings.key, settings.scale, settings.voice2)
    };
    if (pitches[2] == pitches[1])
        pitches[2] = -1;

    for (int v = 0; v < kVoices; ++v)
    {
        voices[(size_t) v] = (int8_t) pitches[v];
        if (pitches[v] < 0)
            continue;

        auto& count = sounding[(size_t) channel][(size_t) pitches[v]];
        if (count++ == 0)
        {
            const auto vel = v == 0 ? velocity
                                    : (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (velocity * settings.velocity));
            out.addEvent (juce::MidiMessage::noteOn (channel + 1, pitches[v], vel), position);
        }
    }
}

void HarmonizeEngine::noteOff (int channel, int note, int position) noexcept
{
    auto& voices = started[(size_t) channel][(size_t) note];
    auto known = false;
    for (auto& pitch : voices)
    {
        if (pitch < 0)
            continue;
        known = true;
        auto& count = sounding[(size_t) channel][(size_t) pitch];
        if (count > 0 && --count == 0)
            out.addEvent (juce::MidiMessage::noteOff (channel + 1, pitch), position);
        pitch = -1;
    }

    // A note this engine never saw (played while bypassed): pass it on.
    if (! known)
        out.addEvent (juce::MidiMessage::noteOff (channel + 1, note), position);
}

void HarmonizeEngine::clearChannel (int channel) noexcept
{
    sounding[(size_t) channel].fill (0);
    for (auto& note : started[(size_t) channel])
        note.fill (-1);
}

void HarmonizeEngine::process (juce::MidiBuffer& midi) noexcept
{
    out.clear();
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto channel = juce::jlimit (0, 15, message.getChannel() - 1);

        if (message.isNoteOn())
            noteOn (channel, message.getNoteNumber(), message.getVelocity(), metadata.samplePosition);
        else if (message.isNoteOff())
            noteOff (channel, message.getNoteNumber(), metadata.samplePosition);
        else
        {
            if (message.isAllNotesOff() || message.isAllSoundOff())
                clearChannel (channel);
            out.addEvent (message, metadata.samplePosition);
        }
    }
    midi.swapWith (out);
}

void HarmonizeEngine::releaseAll (juce::MidiBuffer& midi) noexcept
{
    for (int channel = 0; channel < 16; ++channel)
        for (int pitch = 0; pitch < 128; ++pitch)
            if (sounding[(size_t) channel][(size_t) pitch] > 0)
                midi.addEvent (juce::MidiMessage::noteOff (channel + 1, pitch), 0);
    reset();
}
