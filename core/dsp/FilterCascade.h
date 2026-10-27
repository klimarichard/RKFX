#pragma once

#include "StateVariableFilter.h"

#include <array>
#include <cmath>
#include <complex>

namespace rkfx
{

/** Up to four 12 dB/oct state-variable stages in series, for one or two
    channels: low-pass, high-pass or band-pass at 12 to 48 dB/oct.

    Two layouts:
    - resonant: the first stage carries the resonance (its 1/Q is passed per
      sample), the extra stages are flat Butterworth sections, so resonance
      sounds the same at every slope (RKFilter);
    - Butterworth: all stages together form a true Butterworth filter of
      order 2 x stages, -3 dB at the cutoff (cut filters in the EQs).

    Band-pass stages are normalised to 0 dB at the centre and each adds
    6 dB/oct per side. Zero stages = pass-through (used to switch a cut
    filter off with a crossfade). */
class FilterCascade
{
public:
    enum class Mode { lowpass, highpass, bandpass };

    static constexpr int kMaxStages = 4;
    static constexpr int kMaxChannels = 2;

    void configure (Mode newMode, int newStages, bool resonantFirstStage)
    {
        mode = newMode;
        stages = std::clamp (newStages, 0, kMaxStages);
        resonant = resonantFirstStage;

        for (int s = 0; s < stages; ++s)
            stageK[s] = 1.0f / stageQ (s);
    }

    Mode getMode() const noexcept   { return mode; }
    int getStages() const noexcept  { return stages; }
    bool isResonant() const noexcept { return resonant; }

    bool matches (Mode m, int s, bool r) const noexcept
    {
        return s == stages && (s == 0 || (m == mode && r == resonant));
    }

    void reset() noexcept
    {
        for (auto& channel : filters)
            for (auto& f : channel)
                f.reset();
    }

    /** @param g          StateVariableFilter::gFor (cutoff, sampleRate)
        @param resonantK  1/Q of the first stage (resonant layout only) */
    float process (int channel, float x, float g, float resonantK) noexcept
    {
        auto& f = filters[(size_t) channel];

        for (int s = 0; s < stages; ++s)
        {
            f[(size_t) s].setCoefficients (g, (s == 0 && resonant) ? resonantK : stageK[s]);

            switch (mode)
            {
                case Mode::lowpass:  x = f[(size_t) s].process (x, StateVariableFilter::Type::lowpass); break;
                case Mode::highpass: x = f[(size_t) s].process (x, StateVariableFilter::Type::highpass); break;
                case Mode::bandpass: x = f[(size_t) s].processNormalisedBandpass (x); break;
            }
        }

        return x;
    }

    /** Magnitude at @p omega (radians per sample), for response displays.
        The TPT state-variable filter is the bilinear transform of the analog
        prototype, so it can be evaluated exactly in the analog domain. */
    double magnitude (double omega, double g, double resonantK) const
    {
        const std::complex<double> s (0.0, std::tan (omega * 0.5) / g);
        std::complex<double> h (1.0, 0.0);

        for (int st = 0; st < stages; ++st)
        {
            const auto k = (st == 0 && resonant) ? resonantK : (double) stageK[st];
            const auto den = s * s + k * s + 1.0;
            switch (mode)
            {
                case Mode::lowpass:  h *= 1.0 / den; break;
                case Mode::highpass: h *= s * s / den; break;
                case Mode::bandpass: h *= k * s / den; break;
            }
        }

        return std::abs (h);
    }

private:
    Mode mode = Mode::lowpass;
    int stages = 0;
    bool resonant = false;
    float stageK[kMaxStages] {};
    std::array<std::array<StateVariableFilter, kMaxStages>, kMaxChannels> filters;

    /** Butterworth section Qs for orders 2, 4, 6, 8. */
    static float butterworthQ (int order, int section)
    {
        static constexpr float q[4][4] = {
            { 0.70710678f, 0.0f,        0.0f,        0.0f },
            { 0.54119610f, 1.30656296f, 0.0f,        0.0f },
            { 0.51763809f, 0.70710678f, 1.93185165f, 0.0f },
            { 0.50979558f, 0.60134489f, 0.89997622f, 2.56291545f }
        };
        return q[order / 2 - 1][section];
    }

    float stageQ (int s) const
    {
        if (mode == Mode::bandpass)
            return 0.70710678f;
        if (resonant)
            return s == 0 ? 0.70710678f : butterworthQ (2 * (stages - 1), s - 1);
        return butterworthQ (2 * stages, s);
    }
};

/** A FilterCascade whose layout (mode, slope, on/off) changes by
    crossfading from the old layout to the new one over about 10 ms, so the
    change never clicks. Both run only during the fade. */
class CrossfadingCascade
{
public:
    using Mode = FilterCascade::Mode;

    void prepare (double sampleRate, double fadeSeconds = 0.01)
    {
        fadeLength = std::max (1, (int) (fadeSeconds * sampleRate));
        fadeRemaining = 0;
        for (auto& c : cascades)
            c.reset();
    }

    /** Applies a layout immediately (no fade). */
    void reset (Mode mode, int stages, bool resonant)
    {
        active = 0;
        fadeRemaining = 0;
        cascades[0].configure (mode, stages, resonant);
        target = { mode, stages, resonant };
        for (auto& c : cascades)
            c.reset();
    }

    /** The wanted layout; a fade to it starts at the next beginBlock(). */
    void setTarget (Mode mode, int stages, bool resonant) noexcept { target = { mode, stages, resonant }; }

    const FilterCascade& current() const noexcept { return cascades[(size_t) active]; }

    /** True when nothing would change the signal (switched off, no fade running). */
    bool isPassThrough() const noexcept
    {
        return fadeRemaining == 0 && cascades[(size_t) active].getStages() == 0 && target.stages == 0;
    }

    void beginBlock() noexcept
    {
        if (fadeRemaining == 0 && ! cascades[(size_t) active].matches (target.mode, target.stages, target.resonant))
        {
            auto& next = cascades[(size_t) (1 - active)];
            next.configure (target.mode, target.stages, target.resonant);
            next.reset();
            fadeRemaining = fadeLength;
        }
    }

    float process (int channel, float x, float g, float resonantK) noexcept
    {
        auto y = cascades[(size_t) active].process (channel, x, g, resonantK);

        if (fadeRemaining > 0)
        {
            const auto t = 1.0f - (float) fadeRemaining / (float) fadeLength;
            y += t * (cascades[(size_t) (1 - active)].process (channel, x, g, resonantK) - y);
        }

        return y;
    }

    /** Call once per sample frame, after every channel's process(). */
    void endSample() noexcept
    {
        if (fadeRemaining > 0 && --fadeRemaining == 0)
            active = 1 - active;
    }

private:
    struct Layout { Mode mode = Mode::lowpass; int stages = 0; bool resonant = false; };

    std::array<FilterCascade, 2> cascades;
    Layout target;
    int active = 0, fadeRemaining = 0, fadeLength = 480;
};

} // namespace rkfx
