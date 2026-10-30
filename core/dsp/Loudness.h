#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace rkfx
{

/** Inter-sample peaks: each channel upsampled 4x by a 48-tap windowed-sinc
    polyphase FIR (12 taps a phase), as ITU-R BS.1770 Annex 2 describes;
    process() returns the largest magnitude among a sample's four phases.
    Within about 0.2 dB of the true peak. Allocation-free. */
class TruePeakDetector
{
public:
    static constexpr int kPhases = 4, kTaps = 12;

    TruePeakDetector()
    {
        // h[n] = sinc ((n - centre) / 4) under a Kaiser-like window, split into phases.
        constexpr int length = kPhases * kTaps;
        const double centre = (length - 1) / 2.0;
        for (int n = 0; n < length; ++n)
        {
            const auto x = (n - centre) / kPhases;
            const auto sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (M_PI * x) / (M_PI * x);
            const auto w = 0.5 - 0.5 * std::cos (2.0 * M_PI * (n + 0.5) / length);   // Hann
            coefficients[(size_t) (n % kPhases)][(size_t) (n / kPhases)] = (float) (sinc * w);
        }
        // Each phase sums to 1 (DC passes at unity).
        for (auto& phase : coefficients)
        {
            float sum = 0.0f;
            for (auto c : phase)
                sum += c;
            for (auto& c : phase)
                c /= sum;
        }
        reset();
    }

    void reset() { history.fill (0.0f); position = 0; }

    float process (float x) noexcept
    {
        position = (position + kTaps - 1) % kTaps;
        history[(size_t) position] = x;
        float peak = std::abs (x);
        for (const auto& phase : coefficients)
        {
            float y = 0.0f;
            for (int t = 0; t < kTaps; ++t)
                y += phase[(size_t) t] * history[(size_t) ((position + t) % kTaps)];
            peak = std::max (peak, std::abs (y));
        }
        return peak;
    }

private:
    std::array<std::array<float, kTaps>, kPhases> coefficients {};
    std::array<float, kTaps> history {};
    int position = 0;
};

/** EBU R128 / ITU-R BS.1770-4 loudness of a whole signal (one or two
    channels, weighted 1): K-weighting, 400 ms blocks every 100 ms, the
    absolute gate at -70 LUFS and the relative gate 10 LU below. Also keeps
    every block's (momentary) loudness, the sample peak and the true peak.
    Feed it with process(), then read. Allocates as blocks accumulate. */
class LoudnessMeter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        channels = std::clamp (numChannels, 1, 2);
        hop = std::max (1, (int) std::lround (sampleRate * 0.1));
        designKWeighting (sampleRate);
        reset();
    }

    void reset()
    {
        for (auto& s : states)
            s = {};
        for (auto& tp : truePeaks)
            tp.reset();
        hopEnergy = 0.0;
        hopCount = 0;
        hops.clear();
        blocks.clear();
        samplePeak = truePeak = 0.0f;
    }

    void process (const float* const* x, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                const auto v = x[ch][n];
                samplePeak = std::max (samplePeak, std::abs (v));
                truePeak = std::max (truePeak, truePeaks[(size_t) ch].process (v));
                const auto k = states[(size_t) ch].process (v, shelf, highPass);
                hopEnergy += k * k;
            }
            if (++hopCount == hop)
                endHop();
        }
    }

    /** Integrated loudness (LUFS); -inf (very low) for silence. */
    double getIntegrated() const
    {
        double sum = 0.0;
        int count = 0;
        for (const auto e : blocks)
            if (toLufs (e) > -70.0)
            {
                sum += e;
                ++count;
            }
        if (count == 0)
            return -200.0;
        const auto relative = toLufs (sum / count) - 10.0;
        sum = 0.0;
        count = 0;
        for (const auto e : blocks)
            if (toLufs (e) > -70.0 && toLufs (e) > relative)
            {
                sum += e;
                ++count;
            }
        return count > 0 ? toLufs (sum / count) : -200.0;
    }

    /** Each 400 ms block's loudness (LUFS), one every 100 ms. */
    std::vector<double> getMomentary() const
    {
        std::vector<double> result;
        result.reserve (blocks.size());
        for (const auto e : blocks)
            result.push_back (toLufs (e));
        return result;
    }

    float getSamplePeak() const noexcept { return samplePeak; }
    float getTruePeak() const noexcept { return truePeak; }

    static double toLufs (double meanSquare) { return meanSquare > 1.0e-20 ? -0.691 + 10.0 * std::log10 (meanSquare) : -200.0; }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };

    struct State
    {
        double z1 = 0, z2 = 0, w1 = 0, w2 = 0;

        double process (double x, const Biquad& s, const Biquad& h) noexcept
        {
            // Transposed direct form II, the shelf then the high-pass.
            const auto y = s.b0 * x + z1;
            z1 = s.b1 * x - s.a1 * y + z2;
            z2 = s.b2 * x - s.a2 * y;
            const auto out = h.b0 * y + w1;
            w1 = h.b1 * y - h.a1 * out + w2;
            w2 = h.b2 * y - h.a2 * out;
            return out;
        }
    };

    int channels = 2, hop = 4800, hopCount = 0;
    Biquad shelf, highPass;
    std::array<State, 2> states;
    std::array<TruePeakDetector, 2> truePeaks;
    double hopEnergy = 0.0;
    std::vector<double> hops;      // mean square per channel-sum, per 100 ms
    std::vector<double> blocks;    // per 400 ms block
    float samplePeak = 0.0f, truePeak = 0.0f;

    void endHop()
    {
        hops.push_back (hopEnergy / hop);
        hopEnergy = 0.0;
        hopCount = 0;
        if (hops.size() >= 4)
        {
            const auto n = hops.size();
            blocks.push_back ((hops[n - 1] + hops[n - 2] + hops[n - 3] + hops[n - 4]) / 4.0);
        }
    }

    // The K-weighting filters at any rate (the BS.1770 analog prototypes, as libebur128 designs them).
    void designKWeighting (double rate)
    {
        {
            const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
            const auto k = std::tan (M_PI * f0 / rate);
            const auto vh = std::pow (10.0, gainDb / 20.0);
            const auto vb = std::pow (vh, 0.4996667741545416);
            const auto a0 = 1.0 + k / q + k * k;
            shelf = { (vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
                      2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
        }
        {
            const double f0 = 38.13547087602444, q = 0.5003270373238773;
            const auto k = std::tan (M_PI * f0 / rate);
            const auto a0 = 1.0 + k / q + k * k;
            highPass = { 1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
        }
    }
};

} // namespace rkfx
