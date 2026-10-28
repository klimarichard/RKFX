#pragma once

#include "StateVariableFilter.h"

namespace rkfx
{

/** Zero-latency 4th-order Linkwitz-Riley band split for one channel
    (24 dB/octave). Low = two Butterworth low-pass stages, high = two
    Butterworth high-pass stages, both -6 dB at the crossover; low + high is a
    flat allpass. Used by RKBoard's frequency split, where the two bands go
    through different pedals before they are added back together. */
class LinkwitzRiley4
{
public:
    void prepare (double sampleRate) noexcept
    {
        for (auto* f : { &low1, &low2, &high1, &high2 })
            f->prepare (sampleRate);
    }

    void reset() noexcept
    {
        for (auto* f : { &low1, &low2, &high1, &high2 })
            f->reset();
    }

    /** Call per block (four tan()). */
    void setFrequency (float hz) noexcept
    {
        for (auto* f : { &low1, &low2, &high1, &high2 })
            f->setCutoff (hz);
    }

    void process (float x, float& low, float& high) noexcept
    {
        using Type = StateVariableFilter::Type;
        low  = low2.process (low1.process (x, Type::lowpass), Type::lowpass);
        high = high2.process (high1.process (x, Type::highpass), Type::highpass);
    }

private:
    StateVariableFilter low1, low2, high1, high2;
};

} // namespace rkfx
