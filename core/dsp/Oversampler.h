#pragma once

#include <array>

namespace rkfx
{

/** Polyphase IIR half-band filter for 2x resampling of one channel: two
    branches of first-order allpasses (in the low-rate domain), so it is
    cheap and has a short, minimum-phase-like delay (no linear-phase FIR
    latency). Coefficients from an elliptic half-band design (Valenzuela and
    Constantinides, as in Laurent de Soras' HIIR), computed offline:

    - stage 1: 8 coefficients, transition 0.04: flat to 0.92 x Nyquist,
      99 dB stopband, about 1.8 base-rate samples of delay each way;
    - stage 2: 4 coefficients, transition 0.25: 117 dB stopband (only needs
      to protect the lower half of the band the first stage passes). */
template <int NumCoefs>
class HalfbandFilter
{
public:
    explicit HalfbandFilter (const std::array<double, NumCoefs>& c)
    {
        for (int i = 0; i < NumCoefs; ++i)
            coefs[(size_t) i] = (float) c[(size_t) i];
    }

    void reset() noexcept
    {
        upState = {};
        downState = {};
        downOdd = 0.0f;
    }

    /** One input sample -> two output samples (interpolation, unity gain). */
    void upsample (float x, float& out0, float& out1) noexcept
    {
        out0 = branch (upState, 0, x);
        out1 = branch (upState, 1, x);
    }

    /** Two input samples -> one output sample (decimation). */
    float downsample (float x0, float x1) noexcept
    {
        const auto y = 0.5f * (branch (downState, 0, x0) + downOdd);
        downOdd = branch (downState, 1, x1);
        return y;
    }

private:
    // Per section: previous input and output (low-rate domain).
    using State = std::array<std::array<float, 2>, NumCoefs>;

    std::array<float, NumCoefs> coefs {};
    State upState {}, downState {};
    float downOdd = 0.0f;

    /** Runs @p x through branch 0 (even coefficients) or 1 (odd coefficients). */
    float branch (State& state, int which, float x) noexcept
    {
        for (int i = which; i < NumCoefs; i += 2)
        {
            auto& s = state[(size_t) i];
            const auto y = coefs[(size_t) i] * (x - s[1]) + s[0];
            s[0] = x;
            s[1] = y;
            x = y;
        }
        return x;
    }
};

/** 1x, 2x or 4x oversampling around a per-sample function (a waveshaper),
    for up to two channels. Works sample by sample, so it needs no buffers and
    never allocates. The added latency is reported by latencySamples(). */
class Oversampler
{
public:
    static constexpr int kMaxChannels = 2;

    /** Base-rate samples of delay added by up + down sampling (rounded from
        the filters' low-frequency group delay: 3.6 at 2x, 4.9 at 4x). */
    static int latencySamples (int factor) noexcept { return factor >= 4 ? 5 : factor == 2 ? 4 : 0; }

    void reset() noexcept
    {
        for (auto& c : channels)
        {
            c.stage1.reset();
            c.stage2.reset();
        }
    }

    /** Applies @p shape (float (int channel, int index, float x)) at
        @p factor x the sample rate, in place on @p io. @c index is the
        base-rate sample the oversampled value belongs to. */
    template <typename Shape>
    void process (float* const* io, int numChannels, int numSamples, int factor, Shape&& shape) noexcept
    {
        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            auto* data = io[ch];
            auto& c = channels[(size_t) ch];

            if (factor <= 1)
            {
                for (int i = 0; i < numSamples; ++i)
                    data[i] = shape (ch, i, data[i]);
                continue;
            }

            if (factor == 2)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float a, b;
                    c.stage1.upsample (data[i], a, b);
                    data[i] = c.stage1.downsample (shape (ch, i, a), shape (ch, i, b));
                }
                continue;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                float a, b, a0, a1, b0, b1;
                c.stage1.upsample (data[i], a, b);
                c.stage2.upsample (a, a0, a1);
                c.stage2.upsample (b, b0, b1);
                const auto da = c.stage2.downsample (shape (ch, i, a0), shape (ch, i, a1));
                const auto db = c.stage2.downsample (shape (ch, i, b0), shape (ch, i, b1));
                data[i] = c.stage1.downsample (da, db);
            }
        }
    }

private:
    static constexpr std::array<double, 8> kStage1 {
        0.04063346092419326, 0.1505051290226746, 0.30075705599187408, 0.46077450496145034,
        0.60952431489618808, 0.73850384111885781, 0.84922381039206596, 0.94974278370500276 };
    static constexpr std::array<double, 4> kStage2 {
        0.04245470986526787, 0.17073985049750015, 0.39331989319033134, 0.7457135887202412 };

    struct Channel
    {
        HalfbandFilter<8> stage1 { kStage1 };
        HalfbandFilter<4> stage2 { kStage2 };
    };

    std::array<Channel, kMaxChannels> channels;
};

} // namespace rkfx
