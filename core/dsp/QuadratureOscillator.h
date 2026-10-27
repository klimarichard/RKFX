#pragma once

#include <cmath>

namespace rkfx
{

/** Cosine/sine oscillator as a rotating phasor: one complex multiply per
    sample, no sin/cos calls. The frequency is set by setFrequency(), which is
    the only place trigonometry runs (call it per sub-block, not per sample).
    renormalise() keeps the phasor on the unit circle; call it every few dozen
    samples.

    @tparam Lanes  independent oscillators advanced in lock-step. */
template <int Lanes>
class QuadratureOscillator
{
public:
    void reset (const float* startPhases = nullptr) noexcept
    {
        for (int l = 0; l < Lanes; ++l)
        {
            const auto phase = startPhases != nullptr ? startPhases[l] : 0.0f;
            c[l] = std::cos (phase);
            s[l] = std::sin (phase);
        }
    }

    void setFrequency (int lane, double hz, double sampleRate) noexcept
    {
        const auto w = 2.0 * 3.14159265358979323846 * hz / sampleRate;
        rc[lane] = (float) std::cos (w);
        rs[lane] = (float) std::sin (w);
    }

    float cosine (int lane) const noexcept { return c[lane]; }
    float sine (int lane) const noexcept   { return s[lane]; }

    void advance() noexcept
    {
        for (int l = 0; l < Lanes; ++l)
        {
            const auto nc = c[l] * rc[l] - s[l] * rs[l];
            const auto ns = c[l] * rs[l] + s[l] * rc[l];
            c[l] = nc;
            s[l] = ns;
        }
    }

    /** First-order correction towards magnitude 1 (exact enough when called often). */
    void renormalise() noexcept
    {
        for (int l = 0; l < Lanes; ++l)
        {
            const auto g = 1.5f - 0.5f * (c[l] * c[l] + s[l] * s[l]);
            c[l] *= g;
            s[l] *= g;
        }
    }

    float c[Lanes] {}, s[Lanes] {};

private:
    float rc[Lanes] {}, rs[Lanes] {};
};

} // namespace rkfx
