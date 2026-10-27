#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace rkfx
{

/** Biquad coefficients, normalised so a0 = 1. */
struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    /** Magnitude at @p omega (radians per sample). */
    double magnitude (double omega) const
    {
        const auto z1 = std::polar (1.0, -omega);
        const auto z2 = z1 * z1;
        return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
    }
};

/** Bell and shelf filters that follow their analog prototypes all the way up
    to Nyquist (no "cramping" of high bands), without oversampling and with
    zero latency.

    Method (after M. Vicanek, "Matched Second Order Digital Filters", 2016):
    the poles are the analog poles mapped exactly (impulse invariance), and
    the numerator is solved so the digital magnitude equals the analog one at
    DC, at the band frequency and at Nyquist. Each design is computed in the
    direction where this is most accurate (bell boosts, shelf cuts) and the
    opposite direction is its exact inverse. Within 1 dB of the analog
    response for band frequencies up to 12 kHz at any sample rate, about
    1.5 dB at 16 kHz on 44.1 kHz; the centre gain is always exact.

    Gains are in dB; 0 dB returns an exact identity. */
namespace MatchedDesign
{
    namespace detail
    {
        using Complex = std::complex<double>;
        constexpr double pi = 3.14159265358979323846;

        /** Exact digital poles of s^2 + 2 zeta wp s + wp^2. */
        inline void poles (double zeta, double wp, double& a1, double& a2)
        {
            a2 = std::exp (-2.0 * zeta * wp);
            if (zeta < 1.0)
                a1 = -2.0 * std::exp (-zeta * wp) * std::cos (wp * std::sqrt (1.0 - zeta * zeta));
            else
                a1 = -2.0 * std::exp (-zeta * wp) * std::cosh (wp * std::sqrt (zeta * zeta - 1.0));
        }

        /** Numerator matching |H|^2 at 0, @p wm and pi. */
        template <typename AnalogH>
        BiquadCoefficients match (AnalogH analog, double a1, double a2, double wm)
        {
            const auto hsq = [&] (double w) { return std::norm (analog (Complex (0.0, w))); };

            const auto A0 = (1.0 + a1 + a2) * (1.0 + a1 + a2);
            const auto A1 = (1.0 - a1 + a2) * (1.0 - a1 + a2);
            const auto A2 = -4.0 * a2;

            const auto p1 = std::pow (std::sin (wm * 0.5), 2.0);
            const auto p0 = 1.0 - p1;
            const auto p2 = 4.0 * p0 * p1;

            const auto B0 = A0 * hsq (0.0);
            const auto B1 = A1 * hsq (pi);
            const auto B2 = (hsq (wm) * (A0 * p0 + A1 * p1 + A2 * p2) - B0 * p0 - B1 * p1) / p2;

            const auto s0 = std::sqrt (B0), s1 = std::sqrt (B1);
            const auto w = 0.5 * (s0 + s1);

            BiquadCoefficients c;
            c.b0 = 0.5 * (w + std::sqrt (std::max (0.0, w * w + B2)));
            c.b1 = 0.5 * (s0 - s1);
            c.b2 = w - c.b0;
            c.a1 = a1;
            c.a2 = a2;
            return c;
        }

        /** 1 / H, renormalised. */
        inline BiquadCoefficients invert (const BiquadCoefficients& c)
        {
            BiquadCoefficients r;
            r.b0 = 1.0 / c.b0;
            r.b1 = c.a1 / c.b0;
            r.b2 = c.a2 / c.b0;
            r.a1 = c.b1 / c.b0;
            r.a2 = c.b2 / c.b0;
            return r;
        }

        /** Gains this close to 0 dB are treated as exactly flat. */
        inline bool isFlat (double gainDb) { return std::abs (gainDb) < 1.0e-6; }

        inline double omega (double hz, double sampleRate)
        {
            return 2.0 * pi * std::clamp (hz, 1.0, 0.49 * sampleRate) / sampleRate;
        }
    }

    /** Peaking bell, gain-symmetric: a cut is the exact mirror of the boost,
        and the width between the half-gain points (in dB) is the same at every
        gain, so a band keeps its width however far it is pushed. */
    inline BiquadCoefficients bell (double hz, double gainDb, double q, double sampleRate)
    {
        using namespace detail;
        if (isFlat (gainDb))
            return {};

        const auto w0 = omega (hz, sampleRate);
        const auto a = std::pow (10.0, std::abs (gainDb) / 40.0);   // sqrt of the boost gain

        const auto analog = [=] (Complex s) { return (s * s + s * (a / q) * w0 + w0 * w0) / (s * s + s * w0 / (a * q) + w0 * w0); };

        double a1, a2;
        poles (1.0 / (2.0 * a * q), w0, a1, a2);
        const auto boost = match (analog, a1, a2, w0);
        return gainDb > 0.0 ? boost : invert (boost);
    }

    /** Low shelf: @p gainDb below the corner, 0 dB above. Q 0.707 = no overshoot. */
    inline BiquadCoefficients lowShelf (double hz, double gainDb, double sampleRate, double q = 0.70710678)
    {
        using namespace detail;
        if (isFlat (gainDb))
            return {};

        const auto w0 = omega (hz, sampleRate);
        const auto a = std::pow (10.0, -std::abs (gainDb) / 40.0);   // designed as a cut
        const auto sa = std::sqrt (a);

        const auto analog = [=] (Complex s) { return a * (s * s + (sa / q) * w0 * s + a * w0 * w0) / (a * s * s + (sa / q) * w0 * s + w0 * w0); };

        double a1, a2;
        poles (1.0 / (2.0 * q), w0 / sa, a1, a2);
        const auto cut = match (analog, a1, a2, w0);
        return gainDb < 0.0 ? cut : invert (cut);
    }

    /** High shelf: @p gainDb above the corner, 0 dB below. */
    inline BiquadCoefficients highShelf (double hz, double gainDb, double sampleRate, double q = 0.70710678)
    {
        using namespace detail;
        if (isFlat (gainDb))
            return {};

        const auto w0 = omega (hz, sampleRate);
        const auto a = std::pow (10.0, -std::abs (gainDb) / 40.0);   // designed as a cut
        const auto sa = std::sqrt (a);

        const auto analog = [=] (Complex s) { return a * (a * s * s + (sa / q) * w0 * s + w0 * w0) / (s * s + (sa / q) * w0 * s + a * w0 * w0); };

        double a1, a2;
        poles (1.0 / (2.0 * q), w0 * sa, a1, a2);
        const auto cut = match (analog, a1, a2, w0);
        return gainDb < 0.0 ? cut : invert (cut);
    }
}

/** Transposed direct form II biquad for up to two channels. Coefficients can
    change every sample block without clicks (smooth parameters in, smooth
    coefficients out). */
class Biquad
{
public:
    static constexpr int kMaxChannels = 2;

    void setCoefficients (const BiquadCoefficients& c) noexcept
    {
        b0 = (float) c.b0; b1 = (float) c.b1; b2 = (float) c.b2;
        a1 = (float) c.a1; a2 = (float) c.a2;
    }

    void reset() noexcept
    {
        for (auto& s : state)
            s = { 0.0f, 0.0f };
    }

    float process (int channel, float x) noexcept
    {
        auto& s = state[(size_t) channel];
        const auto y = b0 * x + s[0];
        s[0] = b1 * x - a1 * y + s[1];
        s[1] = b2 * x - a2 * y;
        return y;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    std::array<std::array<float, 2>, kMaxChannels> state {};
};

} // namespace rkfx
