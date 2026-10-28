#pragma once

#include <array>
#include <cstdlib>

namespace rkfx
{

/** Diatonic harmony in a key and scale, shared by RKHarmonize (MIDI) and
    RKVoice (audio), so both harmonize alike.

    - Seven-note scales move diatonically: a "3rd above" is a major or minor
      third as the key requires. A note outside the scale keeps its distance
      from the scale note below it (chromatic passing notes stay parallel).
    - Pentatonic and blues scales: the interval is taken as in a major scale
      and the harmony moves to the nearest note of the scale.
    - Chromatic: fixed intervals (a 3rd is always a major third).
    - Octaves are always 12 semitones. */
namespace scales
{
    enum class Scale
    {
        major, naturalMinor, harmonicMinor, melodicMinor, dorian, phrygian, lydian, mixolydian,
        majorPentatonic, minorPentatonic, blues, chromatic
    };
    inline constexpr int kNumScales = 12;

    /** Harmony intervals: off, then below and above. */
    enum class Interval { off, octaveBelow, sixthBelow, fifthBelow, fourthBelow, thirdBelow,
                          thirdAbove, fourthAbove, fifthAbove, sixthAbove, octaveAbove };
    inline constexpr int kNumIntervals = 11;

    struct ScaleNotes { std::array<int, 7> steps; int size; };

    inline ScaleNotes notesOf (Scale s) noexcept
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
    inline int stepsOf (Interval i) noexcept
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
    inline int semitonesOf (Interval i) noexcept
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

    inline int floorDiv (int a, int b) noexcept { return a >= 0 ? a / b : -((-a + b - 1) / b); }

    /** The harmony for @p note (MIDI note number) in @p key (0 = C .. 11 = B),
        or -1 (off, or outside 0..127). */
    inline int harmonyNote (int note, int key, Scale scale, Interval interval) noexcept
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
}

} // namespace rkfx
