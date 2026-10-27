#pragma once

#include "StateVariableFilter.h"

namespace rkfx
{

/** Zero-latency 2nd-order Linkwitz-Riley band split for one channel.

    Low = LP2 and high = -HP2 of a critically damped (Q 0.5) state-variable
    filter: the high band's polarity is flipped so that low + high is a flat
    allpass (no notch or bump at the crossover). Process the two bands, then
    add them back together. Used by RKTremolo (harmonic mode), RKDeEss,
    RKRotary and RKUtility (bass mono). */
class Crossover
{
public:
    void prepare (double sampleRate) noexcept { filter.prepare (sampleRate); }
    void reset() noexcept { filter.reset(); }

    /** Call per block (one tan()). */
    void setFrequency (float hz) noexcept { filter.setCutoff (hz, 0.5f); }

    void process (float x, float& low, float& high) noexcept
    {
        // One SVF update gives both outputs; low-pass = x - k*bp - hp.
        const auto hp = filter.process (x, StateVariableFilter::Type::highpass);
        low = x - 2.0f * filter.lastBandpass() - hp;
        high = -hp;
    }

private:
    StateVariableFilter filter;
};

} // namespace rkfx
