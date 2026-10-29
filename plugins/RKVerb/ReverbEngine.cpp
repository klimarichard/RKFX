#include "ReverbEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

namespace
{
    constexpr double kTwoPi = 6.28318530717958647692;

    // Line lengths at full size (ms); co-prime-ish so the echoes never line up.
    constexpr float kLineMs[ReverbEngine::kLines] = { 31.7f, 37.3f, 41.9f, 47.1f, 53.9f, 61.3f, 69.7f, 79.1f };
    constexpr float kMaxLineMs = 79.1f;

    // Input diffusers (ms at full size).
    constexpr float kDiffuserMs[ReverbEngine::kDiffusers] = { 4.77f, 3.59f, 12.73f, 9.31f };

    // Early reflections, (ms, gain), at full size. Right channel taps are offset slightly.
    struct Tap { float ms, gain; };
    constexpr Tap kRoomTaps[ReverbEngine::kTaps] = {
        { 7.0f, 0.70f }, { 11.0f, 0.62f }, { 13.5f, 0.55f }, { 17.0f, 0.50f },
        { 19.5f, 0.44f }, { 23.0f, 0.38f }, { 28.5f, 0.33f }, { 31.0f, 0.28f } };
    constexpr Tap kHallTaps[ReverbEngine::kTaps] = {
        { 19.0f, 0.50f }, { 27.0f, 0.45f }, { 37.0f, 0.40f }, { 43.0f, 0.36f },
        { 53.0f, 0.31f }, { 61.0f, 0.27f }, { 71.0f, 0.23f }, { 83.0f, 0.20f } };
    constexpr float kRightTapOffsetMs = 1.3f;

    // Per type: line-length scale, diffusion, reflection levels (room, hall).
    struct TypeShape { float scale, diffusion, room, hall; };
    constexpr TypeShape kTypes[3] = {
        { 0.35f, 0.60f, 1.0f, 0.0f },   // room
        { 1.00f, 0.50f, 0.0f, 0.7f },   // hall
        { 0.55f, 0.75f, 0.0f, 0.0f },   // plate
    };

    // Modulated lines and their LFO rates.
    constexpr int kModLines[4] = { 0, 2, 5, 7 };
    constexpr double kModHz[4] = { 0.31, 0.47, 0.73, 0.91 };

    // Injection and output sign patterns (decorrelate left and right).
    constexpr float kInSign[ReverbEngine::kLines]  = { 1, 1, -1, -1, 1, -1, 1, -1 };
    constexpr float kOutL[ReverbEngine::kLines]    = { 1, -1, 1, -1, 1, -1, 1, -1 };
    constexpr float kOutR[ReverbEngine::kLines]    = { 1, 1, -1, -1, 1, 1, -1, -1 };

    float sizeToFactor (float size) noexcept { return 0.3f + 0.7f * size; }

    // The shimmer path is band-limited, so octaves can't pile up at the bottom
    // or alias at the top.
    constexpr float kShimmerHighPassHz = 80.0f;
    constexpr float kShimmerLowPassHz = 10000.0f;
}

void ReverbEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    const auto msToSamples = [this] (double ms) { return (int) std::ceil (ms * 0.001 * sampleRate) + 8; };

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        predelay[(size_t) ch].prepare (msToSamples (kMaxPredelaySeconds * 1000.0));
        reflections[(size_t) ch].prepare (msToSamples (90.0));
        for (int d = 0; d < kDiffusers; ++d)
            diffusers[(size_t) ch][(size_t) d].prepare (msToSamples (kDiffuserMs[d]));
        lowCut[(size_t) ch].prepare (sampleRate);
        highCut[(size_t) ch].prepare (sampleRate);
    }

    for (int i = 0; i < kLines; ++i)
    {
        lines[(size_t) i].prepare (msToSamples (kMaxLineMs + 2.0));
        lineDc[(size_t) i].prepare (sampleRate, 5.0);
    }

    for (auto* s : { &predelaySamples, &sizeFactor, &typeScale, &diffusion, &roomLevel, &hallLevel, &modSamples, &width })
        s->reset (sampleRate, 0.05);   // size and type glide over 50 ms
    freeze.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    shimmer.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    shimmerShifter.prepare (sampleRate);
    shimmerHighPass.prepare (sampleRate);
    shimmerLowPass.prepare (sampleRate);
    shimmerHighPass.setCutoff (kShimmerHighPassHz);
    shimmerLowPass.setCutoff (kShimmerLowPassHz);

    setSettings (settings);
    reset();
}

void ReverbEngine::reset()
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        predelay[(size_t) ch].reset();
        reflections[(size_t) ch].reset();
        for (auto& d : diffusers[(size_t) ch])
            d.reset();
        lowCut[(size_t) ch].reset();
        highCut[(size_t) ch].reset();
    }
    for (int i = 0; i < kLines; ++i)
    {
        lines[(size_t) i].reset();
        lineDc[(size_t) i].reset();
    }
    damped.fill (0.0f);
    modPhase = { 0.0, 0.25, 0.5, 0.75 };
    shimmerShifter.reset();
    shimmerHighPass.reset();
    shimmerLowPass.reset();
    shimmerRunning = false;

    for (auto* s : { &predelaySamples, &sizeFactor, &typeScale, &diffusion, &roomLevel, &hallLevel, &modSamples, &width, &freeze, &shimmer })
        s->setCurrentAndTargetValue (s->getTargetValue());
}

void ReverbEngine::setSettings (const Settings& s)
{
    settings = s;
    const auto& shape = kTypes[(int) s.type];

    predelaySamples.setTargetValue (juce::jlimit (0.0f, (float) kMaxPredelaySeconds * 1000.0f, s.predelayMs) * 0.001f * (float) sampleRate);
    sizeFactor.setTargetValue (sizeToFactor (juce::jlimit (0.0f, 1.0f, s.size)));
    typeScale.setTargetValue (shape.scale);
    diffusion.setTargetValue (shape.diffusion);
    roomLevel.setTargetValue (shape.room);
    hallLevel.setTargetValue (shape.hall);
    modSamples.setTargetValue (juce::jlimit (0.0f, 1.0f, s.modulation) * 0.0006f * (float) sampleRate);   // up to +/-0.6 ms
    width.setTargetValue (juce::jlimit (0.0f, 1.0f, s.width));
    freeze.setTargetValue (s.freeze ? 1.0f : 0.0f);
    shimmer.setTargetValue (juce::jlimit (0.0f, 1.0f, s.shimmer));
    shimmerShifter.setRatio (juce::jlimit (0.25f, 4.0f, s.shimmerRatio));

    // Measured decay runs about 6% short of the loop-gain formula (energy taken by the
    // loops' DC blockers and the diffusion); calibrated so the knob reads true.
    constexpr float kDecayCalibration = 1.065f;
    decaySeconds = juce::jlimit (0.1f, 60.0f, s.decaySeconds) * kDecayCalibration;

    // Damping: a low-pass in each loop, from fully open (0%) down to 1.5 kHz (100%).
    const auto cutoff = 20000.0 * std::pow (1500.0 / 20000.0, (double) juce::jlimit (0.0f, 1.0f, s.damping));
    dampingCoef = s.damping <= 0.0f ? 0.0f : (float) std::exp (-kTwoPi * juce::jmin (cutoff, 0.45 * sampleRate) / sampleRate);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        lowCut[(size_t) ch].setCutoff (juce::jlimit (10.0f, 20000.0f, s.lowCutHz));
        highCut[(size_t) ch].setCutoff (juce::jlimit (200.0f, 20000.0f, s.highCutHz));
    }
}

void ReverbEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    const auto msToSamples = (float) (0.001 * sampleRate);
    const auto sr = (float) sampleRate;

    // Loop gains for the current line lengths (size and type glide slowly: once per block is enough).
    float lineGain[kLines];
    {
        const auto scaleNow = sizeFactor.getCurrentValue() * typeScale.getCurrentValue();
        for (int l = 0; l < kLines; ++l)
            lineGain[l] = std::pow (10.0f, -3.0f * kLineMs[l] * scaleNow * 0.001f / decaySeconds);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto pd = predelaySamples.getNextValue();
        const auto scale = sizeFactor.getNextValue() * typeScale.getNextValue();
        const auto g = diffusion.getNextValue();
        const auto room = roomLevel.getNextValue();
        const auto hall = hallLevel.getNextValue();
        const auto mod = modSamples.getNextValue();
        const auto w = width.getNextValue();
        const auto frozen = freeze.getNextValue();

        // Pre-delay, early reflections and diffusion, per input channel.
        float diffused[kMaxChannels] {}, early[kMaxChannels] {};
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            const auto x = io[juce::jmin (ch, numChannels - 1)][i] * (1.0f - frozen);

            auto& pre = predelay[(size_t) ch];
            pre.write (x);
            const auto p = pd < 1.0f ? x : pre.read (pd);

            auto& refl = reflections[(size_t) ch];
            refl.write (p);
            const auto offset = ch == 0 ? 0.0f : kRightTapOffsetMs;
            float er = 0.0f;
            if (room > 1.0e-4f)
                for (const auto& t : kRoomTaps)
                    er += room * t.gain * refl.read ((t.ms + offset) * sizeFactor.getCurrentValue() * msToSamples);
            if (hall > 1.0e-4f)
                for (const auto& t : kHallTaps)
                    er += hall * t.gain * refl.read ((t.ms + offset) * sizeFactor.getCurrentValue() * msToSamples);
            early[ch] = er;

            // Schroeder allpasses: v = x + g d; y = d - g v.
            auto d = p;
            for (int k = 0; k < kDiffusers; ++k)
            {
                auto& line = diffusers[(size_t) ch][(size_t) k];
                const auto delayed = line.read (juce::jmax (2.0f, kDiffuserMs[k] * (0.5f + 0.5f * sizeFactor.getCurrentValue()) * msToSamples) - 1.0f);
                const auto v = d + g * delayed;
                line.write (v);
                d = delayed - g * v;
            }
            diffused[ch] = d;
        }

        // Feedback delay network.
        float out[kLines];
        for (int l = 0; l < kLines; ++l)
        {
            auto len = kLineMs[l] * scale * msToSamples;
            for (int m = 0; m < 4; ++m)
                if (kModLines[m] == l)
                    len += mod * (float) std::sin (kTwoPi * modPhase[(size_t) m]);
            out[l] = lines[(size_t) l].read (juce::jmax (2.0f, len) - 1.0f);
        }

        // Outputs: decorrelated sums of the lines.
        float lateL = 0.0f, lateR = 0.0f;
        for (int l = 0; l < kLines; ++l)
        {
            lateL += kOutL[l] * out[l];
            lateR += kOutR[l] * out[l];
        }

        // Shimmer: the tail, shifted and band-limited, goes back into the network.
        auto shimmerFeed = 0.0f;
        const auto shimmerAmount = shimmer.getNextValue();
        if (shimmerAmount > 0.0f || shimmer.isSmoothing())
        {
            if (! shimmerRunning)
            {
                shimmerShifter.reset();
                shimmerHighPass.reset();
                shimmerLowPass.reset();
                shimmerRunning = true;
            }

            using Type = rkfx::StateVariableFilter::Type;
            auto shifted = shimmerShifter.process (0.35f * lateL);
            shifted = shimmerLowPass.process (shimmerHighPass.process (shifted, Type::highpass), Type::lowpass);
            shimmerFeed = rkfx::softClip (shimmerAmount * kShimmerFeed * shifted * (1.0f - frozen));
        }
        else
        {
            shimmerRunning = false;
        }

        float sum = 0.0f, v[kLines];
        const auto dampingNow = dampingCoef * (1.0f - frozen);
        for (int l = 0; l < kLines; ++l)
        {
            // Loop gain for this line's length: -60 dB after `decaySeconds`; unity when frozen.
            const auto gain = lineGain[l];
            auto& lp = damped[(size_t) l];
            lp = out[l] + dampingNow * (lp - out[l]);
            v[l] = lp * (gain + frozen * (1.0f - gain));
            sum += v[l];
        }

        const auto householder = 2.0f / (float) kLines * sum;
        for (int l = 0; l < kLines; ++l)
        {
            const auto feedback = v[l] - householder;
            const auto injected = 0.5f * kInSign[l] * (diffused[l % 2] + shimmerFeed);
            const auto in = rkfx::softClip (lineDc[(size_t) l].process (feedback + injected), 4.0f);
            lines[(size_t) l].write (in);
        }

        for (int m = 0; m < 4; ++m)
        {
            modPhase[(size_t) m] += kModHz[m] / sr;
            if (modPhase[(size_t) m] >= 1.0)
                modPhase[(size_t) m] -= 1.0;
        }

        // Outputs, plus the early reflections.
        float wet[kMaxChannels] = { 0.35f * lateL + early[0], 0.35f * lateR + early[1] };

        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            wet[ch] = lowCut[(size_t) ch].process (wet[ch], rkfx::StateVariableFilter::Type::highpass);
            wet[ch] = highCut[(size_t) ch].process (wet[ch], rkfx::StateVariableFilter::Type::lowpass);
        }

        const auto mid = 0.5f * (wet[0] + wet[1]);
        const auto side = 0.5f * (wet[0] - wet[1]) * w;

        if (numChannels == 2)
        {
            io[0][i] = mid + side;
            io[1][i] = mid - side;
        }
        else if (numChannels == 1)
        {
            io[0][i] = mid;
        }
    }
}
