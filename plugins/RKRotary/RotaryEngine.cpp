#include "RotaryEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

namespace
{
    constexpr float kTwoPi = 6.28318530718f;
    constexpr float kHalfPi = 1.57079632679f;

    // Doppler: a delay swinging around a base, in ms.
    constexpr float kHornBaseMs = 1.2f, kHornSwingMs = 0.45f;
    constexpr float kDrumBaseMs = 1.5f, kDrumSwingMs = 0.6f;

    // Amplitude modulation depth (close mics) and horn darkening when it faces away.
    constexpr float kHornAm = 0.55f, kDrumAm = 0.3f;
    constexpr double kHornFacingHz = 12000.0, kHornAwayHz = 2500.0;

    constexpr float kTubeBias = 0.25f;
    constexpr float kDriveMaxDb = 24.0f;
}

void RotaryEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    crossover.prepare (sampleRate);
    crossover.setFrequency (kCrossoverHz);
    driveDc.prepare (sampleRate);

    const auto ms = [this] (float m) { return (int) std::ceil (m * 0.001 * sampleRate) + 8; };
    horn.line.prepare (ms (kHornBaseMs + kHornSwingMs + 1.0f));
    drum.line.prepare (ms (kDrumBaseMs + kDrumSwingMs + 1.0f));
    cleanDelay.prepare (16);
    scratch.assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);

    const auto onePole = [this] (double hz) { return (float) std::exp (-kTwoPi * juce::jmin (hz, 0.45 * sampleRate) / sampleRate); };
    darkCoefFacing = onePole (kHornFacingHz);
    darkCoefAway = onePole (kHornAwayHz);

    drive.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    balance.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    distance.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    width.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void RotaryEngine::reset()
{
    crossover.reset();
    oversampler.reset();
    cleanDelay.reset();
    driveDc.reset();
    horn.line.reset();
    drum.line.reset();
    horn.angle = 0.0;
    drum.angle = kHalfPi;   // the rotors never start lined up
    horn.lowPass = {};
    drum.lowPass = {};
    factor = settings.oversampling;

    // Start at the set speed (no spin-up after loading a preset).
    const double target = settings.speed == Speed::fast ? settings.fastHz : settings.speed == Speed::slow ? settings.slowHz : 0.0f;
    hornHz = target;
    drumHz = target * kDrumRatio;

    for (auto* s : { &drive, &balance, &distance, &width })
        s->setCurrentAndTargetValue (s->getTargetValue());
}

void RotaryEngine::setSettings (const Settings& s)
{
    settings = s;
    settings.oversampling = s.oversampling >= 4 ? 4 : s.oversampling >= 2 ? 2 : 1;

    hornAccelCoef = std::exp (-1.0 / (juce::jlimit (0.05f, 20.0f, s.hornAccelSeconds) * sampleRate));
    drumAccelCoef = std::exp (-1.0 / (juce::jlimit (0.05f, 20.0f, s.drumAccelSeconds) * sampleRate));

    drive.setTargetValue (juce::jlimit (0.0f, 1.0f, s.drive));
    balance.setTargetValue (juce::jlimit (-1.0f, 1.0f, s.balance));
    distance.setTargetValue (juce::jlimit (0.0f, 1.0f, s.distance));
    width.setTargetValue (juce::jlimit (0.0f, 1.0f, s.width));
}

void RotaryEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0)
        return;

    if (settings.oversampling != factor)
    {
        factor = settings.oversampling;
        oversampler.reset();
    }

    const auto chunk = (int) scratch.size();
    for (int start = 0; start < numSamples; start += chunk)
    {
        float* ptrs[2] = { io[0] + start, io[juce::jmin (1, numChannels - 1)] + start };
        processChunk (ptrs, numChannels, juce::jmin (chunk, numSamples - start));
    }
}

void RotaryEngine::processChunk (float* const* io, int numChannels, int numSamples) noexcept
{
    auto* mono = scratch.data();

    // Mono sum: a rotary cabinet has one input.
    for (int i = 0; i < numSamples; ++i)
        mono[i] = numChannels > 1 ? 0.5f * (io[0][i] + io[1][i]) : io[0][i];

    // Drive (tube curve, oversampled). Skipped at 0%; then a plain delay keeps the latency the same.
    const auto driving = drive.isSmoothing() || drive.getTargetValue() > 0.0f;
    if (driving && ! wasDriving)
        oversampler.reset();   // don't replay stale samples from the last time it ran
    wasDriving = driving;

    if (driving)
    {
        // Per base-rate sample: curve gain, level match and blend from clean (gentle near 0%).
        float gainIn[64], gainOut[64], blend[64];
        for (int start = 0; start < numSamples; start += 64)
        {
            const auto n = juce::jmin (64, numSamples - start);
            for (int i = 0; i < n; ++i)
            {
                const auto amount = drive.getNextValue();
                gainIn[i] = std::pow (10.0f, amount * kDriveMaxDb / 20.0f);
                gainOut[i] = std::pow (10.0f, -0.75f * amount * kDriveMaxDb / 20.0f);
                blend[i] = juce::jmin (1.0f, amount * 20.0f);
            }

            float* block = mono + start;
            oversampler.process (&block, 1, n, factor, [&] (int, int index, float x)
            {
                static const auto slope = (rkfx::softClip (kTubeBias + 1.0e-3f) - rkfx::softClip (kTubeBias - 1.0e-3f)) / 2.0e-3f;
                const auto shaped = gainOut[index] * (rkfx::softClip (x * gainIn[index] + kTubeBias) - rkfx::softClip (kTubeBias)) / slope;
                return x + blend[index] * (shaped - x);
            });

            for (int i = 0; i < n; ++i)
                block[i] = driveDc.process (block[i]);   // the tube curve is asymmetric
        }
    }
    else
    {
        const auto latency = latencySamples();
        for (int i = 0; i < numSamples; ++i)
        {
            cleanDelay.write (mono[i]);
            if (latency > 0)
                mono[i] = cleanDelay.read ((float) latency);
        }
    }

    const auto rateToStep = 2.0 * juce::MathConstants<double>::pi / sampleRate;
    const auto msToSamples = (float) (0.001 * sampleRate);
    const double targetHz = settings.speed == Speed::fast ? settings.fastHz : settings.speed == Speed::slow ? settings.slowHz : 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Rotor speeds ease towards the target; positions are integrated every sample.
        hornHz = targetHz + hornAccelCoef * (hornHz - targetHz);
        drumHz = targetHz * kDrumRatio + drumAccelCoef * (drumHz - targetHz * kDrumRatio);
        horn.angle += hornHz * rateToStep;
        drum.angle += drumHz * rateToStep;
        if (horn.angle > kTwoPi) horn.angle -= 2.0 * juce::MathConstants<double>::pi;
        if (drum.angle > kTwoPi) drum.angle -= 2.0 * juce::MathConstants<double>::pi;

        const auto bal = balance.getNextValue();
        const auto dist = distance.getNextValue();
        const auto w = width.getNextValue();

        float low, high;
        crossover.process (mono[i], low, high);
        horn.line.write (high);
        drum.line.write (low);

        // Close mics hear the full movement; distant ones less of it.
        const auto closeness = 1.0f - 0.6f * dist;
        float mic[2] {};

        for (int m = 0; m < 2; ++m)
        {
            // The two mics sit on opposite sides of the cabinet.
            const auto offset = m == 0 ? 0.0f : 3.14159265f;

            {
                const auto a = (float) horn.angle + offset;
                const auto facing = 0.5f + 0.5f * std::cos (a);
                const auto delay = (kHornBaseMs + closeness * kHornSwingMs * std::sin (a)) * msToSamples;
                auto v = horn.line.read (delay);

                // Darker when the horn points away (less so for a distant mic).
                const auto darkness = closeness * (1.0f - facing);
                const auto coef = darkCoefFacing + darkness * (darkCoefAway - darkCoefFacing);
                auto& lp = horn.lowPass[(size_t) m];
                lp = v + coef * (lp - v);

                v = lp * (1.0f - closeness * kHornAm * (1.0f - facing)) / (1.0f - 0.5f * closeness * kHornAm);
                mic[m] += juce::jmin (1.0f, 1.0f + bal) * v;
            }
            {
                const auto a = (float) drum.angle + offset;
                const auto facing = 0.5f + 0.5f * std::cos (a);
                const auto delay = (kDrumBaseMs + closeness * kDrumSwingMs * std::sin (a)) * msToSamples;
                const auto v = drum.line.read (delay) * (1.0f - closeness * kDrumAm * (1.0f - facing)) / (1.0f - 0.5f * closeness * kDrumAm);
                mic[m] += juce::jmin (1.0f, 1.0f - bal) * v;
            }
        }

        const auto mid = 0.5f * (mic[0] + mic[1]);
        const auto side = 0.5f * (mic[0] - mic[1]) * w;

        if (numChannels > 1)
        {
            io[0][i] = mid + side;
            io[1][i] = mid - side;
        }
        else
        {
            io[0][i] = mid;
        }
    }
}
