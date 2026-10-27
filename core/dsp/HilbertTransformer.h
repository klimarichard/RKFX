#pragma once

#include <array>
#include <cmath>

namespace rkfx
{

/** Hilbert transformer as two parallel chains of six first-order allpass
    filters. For any input, the two outputs (I and Q) have the same magnitude
    and Q lags I by 90 degrees, within 0.58 degrees from 20 Hz to 20 kHz at
    every sample rate from 44.1 to 192 kHz (unwanted sideband about -46 dB in
    a frequency shifter). Zero latency: both outputs share the same, purely
    phase-shifted, timing.

    Design: an analog phase-difference network with 12 real poles, optimised
    once for minimax phase error over a 5000:1 band (scale invariant), then
    centred on each sample rate's prewarped 20 Hz to 20 kHz band and mapped to
    digital allpasses with the bilinear transform.

    @tparam Lanes  independent signals processed in lock-step (for example the
                   four shifters' feedback paths), laid out so the compiler
                   can vectorise across lanes. */
template <int Lanes>
class HilbertTransformer
{
public:
    static constexpr int kStagesPerChain = 6;

    void prepare (double sampleRate)
    {
        // Normalised analog pole frequencies, sorted; even indices form the
        // Q chain, odd indices the I chain.
        static constexpr double poles[2 * kStagesPerChain] = {
            0.006003797822711602, 0.022332750031336952, 0.054729189235683505, 0.1265598948170534,
            0.2896711729267925,   0.6617284775771083,   1.5111938414097865,   3.4521902538528613,
            7.901397211537935,    18.271785384827126,   44.777288896209214,   166.56123832436987
        };

        constexpr double pi = 3.14159265358979323846;
        const auto lo = std::tan (pi * 20.0 / sampleRate);
        const auto hi = std::tan (pi * std::min (20000.0, 0.45 * sampleRate) / sampleRate);
        const auto centre = std::sqrt (lo * hi);

        for (int k = 0; k < kStagesPerChain; ++k)
        {
            const auto pq = centre * poles[2 * k];
            const auto pi_ = centre * poles[2 * k + 1];
            coefQ[(size_t) k] = (float) ((pq - 1.0) / (pq + 1.0));
            coefI[(size_t) k] = (float) ((pi_ - 1.0) / (pi_ + 1.0));
        }

        reset();
    }

    void reset() noexcept
    {
        for (auto& s : stateI) s.fill (0.0f);
        for (auto& s : stateQ) s.fill (0.0f);
    }

    /** Processes one sample per lane. @p in, @p outI and @p outQ hold Lanes values. */
    void process (const float* in, float* outI, float* outQ) noexcept
    {
        float i[Lanes], q[Lanes];
        for (int l = 0; l < Lanes; ++l)
            i[l] = q[l] = in[l];

        for (int k = 0; k < kStagesPerChain; ++k)
        {
            runStage (coefI[(size_t) k], stateI[(size_t) k], i);
            runStage (coefQ[(size_t) k], stateQ[(size_t) k], q);
        }

        for (int l = 0; l < Lanes; ++l)
        {
            outI[l] = i[l];
            outQ[l] = q[l];
        }
    }

private:
    // Per stage: x[n-1] and y[n-1] for each lane.
    using StageState = std::array<float, 2 * Lanes>;

    std::array<float, kStagesPerChain> coefI {}, coefQ {};
    std::array<StageState, kStagesPerChain> stateI {}, stateQ {};

    /** First-order allpass (c + z^-1) / (1 + c z^-1), in place on every lane. */
    static void runStage (float c, StageState& s, float* x) noexcept
    {
        for (int l = 0; l < Lanes; ++l)
        {
            const auto in = x[l];
            const auto y = c * (in - s[(size_t) (Lanes + l)]) + s[(size_t) l];
            s[(size_t) l] = in;
            s[(size_t) (Lanes + l)] = y;
            x[l] = y;
        }
    }
};

} // namespace rkfx
