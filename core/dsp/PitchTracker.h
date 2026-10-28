#pragma once

#include "EnvelopeFollower.h"
#include "StateVariableFilter.h"

#include <algorithm>

namespace rkfx
{

/** Zero-latency monophonic pitch tracking, as in analog octave dividers and
    guitar synths (RKOctave, RKSynth).

    The input is band-limited to the instrument's range: a high-pass below
    its lowest note, and a 24 dB/oct low-pass that follows the detected
    pitch (1.25 x the fundamental), so the fundamental dominates and the
    harmonics add no zero crossings. Before the first cycle is measured,
    the low-pass sits just above the lowest note: whatever is played, its
    fundamental is then the strongest component left. A Schmitt trigger
    (hysteresis at 30% of the filtered level) finds one rising crossing per
    cycle; the period between them is the pitch.

    Call process() once per sample; it reports each new cycle. The pitch is
    known after two cycles of a note (one cycle's length later than the
    note itself, which no tracker can avoid). Single notes only: chords and
    double stops confuse it. */
class PitchTracker
{
public:
    enum class Range { bass, guitar, violin };

    struct Limits { float lowHz, highHz; };

    /** Lowest and highest fundamental a range tracks, in Hz. */
    static Limits limitsFor (Range r) noexcept
    {
        switch (r)
        {
            case Range::bass:   return { 35.0f, 400.0f };     // B0 to the 20th fret of the G string
            case Range::guitar: return { 60.0f, 1100.0f };    // drop C (and cello) to the top frets
            case Range::violin: return { 125.0f, 2700.0f };   // C3 (5-string violin, viola) to E7
        }
        return { 60.0f, 1100.0f };
    }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        for (auto* f : { &highPass, &low1, &low2 })
            f->prepare (sampleRate);
        level.prepare (sampleRate);
        level.setTimes (0.5f, 30.0f);
        reset();
    }

    void setRange (Range r) noexcept
    {
        if (r != range)
        {
            range = r;
            reset();
        }
    }

    /** Forgets the note (call between notes, after silence). */
    void reset() noexcept
    {
        limits = limitsFor (range);
        for (auto* f : { &highPass, &low1, &low2 })
            f->reset();
        level.reset();
        high = false;
        samplesSinceRise = 0;
        hz = 0.0f;
        band = 0.0f;
        highPass.setCutoff (limits.lowHz * 0.6f);
        updateCoefficients();
    }

    /** True when a new cycle starts (a rising crossing of the fundamental). */
    bool process (float x) noexcept
    {
        if (--untilUpdate <= 0)
            updateCoefficients();

        band = highPass.process (x, StateVariableFilter::Type::highpass);
        const auto y = low2.process (low1.process (band, StateVariableFilter::Type::lowpass), StateVariableFilter::Type::lowpass);
        const auto threshold = 0.3f * level.process (y);

        samplesSinceRise = std::min (samplesSinceRise + 1, 1 << 20);

        if (! high && y > threshold && threshold > 1.0e-7f)
        {
            high = true;

            const auto measured = (float) sampleRate / (float) samplesSinceRise;
            samplesSinceRise = 0;
            if (measured >= limits.lowHz * 0.5f && measured <= limits.highHz * 1.5f)
                hz = hz > 0.0f ? 0.5f * (hz + measured) : measured;
            return true;
        }

        if (high && y < -threshold)
            high = false;
        return false;
    }

    /** The tracked fundamental in Hz, 0 until the first cycle is measured. */
    float getHz() const noexcept { return hz; }

    /** The high-passed input of the last process() call (it has the input's
        own zero crossings, without rumble). */
    float getBand() const noexcept { return band; }

    const Limits& getLimits() const noexcept { return limits; }

private:
    static constexpr int kUpdateInterval = 16;

    double sampleRate = 48000.0;
    Range range = Range::guitar;
    Limits limits = limitsFor (Range::guitar);
    StateVariableFilter highPass, low1, low2;
    EnvelopeFollower level;
    bool high = false;
    int samplesSinceRise = 0, untilUpdate = 0;
    float hz = 0.0f, band = 0.0f;

    void updateCoefficients() noexcept
    {
        const auto fundamental = hz > 0.0f ? hz : limits.lowHz;
        const auto cutoff = std::clamp (fundamental, limits.lowHz, limits.highHz) * 1.25f;
        const auto g = StateVariableFilter::gFor (cutoff, sampleRate);
        low1.setCoefficients (g, 1.0f / 0.70710678f);
        low2.setCoefficients (g, 1.0f / 0.70710678f);
        untilUpdate = kUpdateInterval;
    }
};

} // namespace rkfx
