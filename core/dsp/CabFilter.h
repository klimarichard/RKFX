#pragma once

#include "FilterCascade.h"
#include "MatchedBiquad.h"
#include "StateVariableFilter.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace rkfx
{

/** Speaker cabinet curves, for drives and preamps going straight to a PA. */
enum class CabType { off, small, medium, large, bass };

/** A speaker-cabinet curve: high-pass, three resonance bells, low-pass. A
    fixed filter, not an impulse response (zero latency, very little CPU).
    Shared by RKDistort and RKPreamp, so their cabinets sound the same. */
class CabFilter
{
public:
    void configure (CabType cab, double rate)
    {
        off = cab == CabType::off;
        if (off)
            return;

        const auto curve = curveFor (cab);
        highPass.configure (FilterCascade::Mode::highpass, curve.highPassStages, false);
        lowPass.configure (FilterCascade::Mode::lowpass, curve.lowPassStages, false);
        highPassG = StateVariableFilter::gFor (curve.highPassHz, rate);
        lowPassG = StateVariableFilter::gFor (curve.lowPassHz, rate);

        for (size_t i = 0; i < 3; ++i)
        {
            const auto& b = curve.bells[i];
            bellCoefs[i] = MatchedDesign::bell (b.hz, b.gainDb, b.q, rate);
            bells[i].setCoefficients (bellCoefs[i]);
        }
    }

    void reset()
    {
        highPass.reset();
        lowPass.reset();
        for (auto& b : bells)
            b.reset();
    }

    float process (int channel, float x) noexcept
    {
        if (off)
            return x;

        x = highPass.process (channel, x, highPassG, 0.0f);
        for (auto& b : bells)
            x = b.process (channel, x);
        return lowPass.process (channel, x, lowPassG, 0.0f);
    }

    bool isOff() const noexcept { return off; }

    /** Response in dB (for tests and documentation). */
    double magnitudeDb (double hz, double rate) const
    {
        if (off)
            return 0.0;

        const auto w = 2.0 * 3.14159265358979323846 * hz / rate;
        auto m = highPass.magnitude (w, highPassG, 0.0) * lowPass.magnitude (w, lowPassG, 0.0);
        for (const auto& c : bellCoefs)
            m *= c.magnitude (w);
        return 20.0 * std::log10 (std::max (m, 1.0e-10));
    }

private:
    struct Curve
    {
        double highPassHz; int highPassStages;
        double lowPassHz;  int lowPassStages;
        struct Bell { double hz, gainDb, q; } bells[3];
    };

    static Curve curveFor (CabType cab)
    {
        switch (cab)
        {
            case CabType::small:  return { 100.0, 1, 5000.0, 2, { { 120.0,  4.0, 1.5 }, { 2500.0, 3.0, 1.0 }, { 800.0, -1.5, 0.8 } } };
            case CabType::medium: return { 80.0,  1, 5500.0, 2, { { 110.0,  3.0, 1.2 }, { 2000.0, 2.5, 1.0 }, { 500.0, -1.0, 0.8 } } };
            case CabType::large:  return { 70.0,  1, 4500.0, 2, { { 100.0,  4.0, 1.2 }, { 2200.0, 3.0, 1.2 }, { 400.0, -3.0, 1.0 } } };
            case CabType::bass:   return { 45.0,  1, 3500.0, 2, { { 80.0,   3.0, 1.0 }, { 1200.0, 1.5, 1.0 }, { 600.0, -2.0, 1.0 } } };
            case CabType::off:    break;
        }
        return {};
    }

    bool off = true;
    FilterCascade highPass, lowPass;
    float highPassG = 0.0f, lowPassG = 0.0f;
    std::array<Biquad, 3> bells;
    std::array<BiquadCoefficients, 3> bellCoefs;
};

} // namespace rkfx
