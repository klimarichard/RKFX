#include "FreqShiftEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

void FreqShiftEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& c : channels)
    {
        c.input.prepare (sampleRate);
        c.feedback.prepare (sampleRate);
        for (auto& d : c.dc)
            d.prepare (sampleRate);
        c.lowCut.prepare (sampleRate);
        c.highCut.prepare (sampleRate);
    }

    for (int k = 0; k < kShifts; ++k)
    {
        hz[(size_t) k].reset (sampleRate, rkfx::kParamSmoothingSeconds);
        down[(size_t) k].reset (sampleRate, rkfx::kParamSmoothingSeconds);
        feedback[(size_t) k].reset (sampleRate, rkfx::kParamSmoothingSeconds);
        gain[(size_t) k].reset (sampleRate, rkfx::kParamSmoothingSeconds);
    }
    spread.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    lowCutHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    highCutHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    reset();
}

void FreqShiftEngine::reset()
{
    // Jump every glide to its target: a reset starts from the current settings.
    for (int k = 0; k < kShifts; ++k)
    {
        hz[(size_t) k].setCurrentAndTargetValue (hz[(size_t) k].getTargetValue());
        down[(size_t) k].setCurrentAndTargetValue (down[(size_t) k].getTargetValue());
        feedback[(size_t) k].setCurrentAndTargetValue (feedback[(size_t) k].getTargetValue());
        gain[(size_t) k].setCurrentAndTargetValue (gain[(size_t) k].getTargetValue());
    }
    spread.setCurrentAndTargetValue (spread.getTargetValue());
    lowCutHz.setCurrentAndTargetValue (lowCutHz.getTargetValue());
    highCutHz.setCurrentAndTargetValue (highCutHz.getTargetValue());

    for (auto& c : channels)
    {
        c.input.reset();
        c.osc.reset();
        c.lowCut.reset();
        c.highCut.reset();
    }

    resetFeedback();
}

void FreqShiftEngine::resetFeedback()
{
    for (auto& c : channels)
    {
        c.feedback.reset();
        for (auto& d : c.dc)
            d.reset();
        std::fill (std::begin (c.lastOut), std::end (c.lastOut), 0.0f);
    }
    feedbackRunning = false;
}

void FreqShiftEngine::setShift (int index, const ShiftSettings& s)
{
    const auto k = (size_t) index;
    hz[k].setTargetValue (juce::jmax (0.0f, s.hz));
    down[k].setTargetValue (juce::jlimit (0.0f, 1.0f, s.down));
    feedback[k].setTargetValue (juce::jlimit (0.0f, 0.9f, s.feedback));
    gain[k].setTargetValue (s.on ? juce::jlimit (0.0f, 1.0f, s.level) : 0.0f);
}

void FreqShiftEngine::setSpread (float spread01)  { spread.setTargetValue (juce::jlimit (0.0f, 1.0f, spread01)); }
void FreqShiftEngine::setLowCut (float hzValue)   { lowCutHz.setTargetValue (juce::jlimit (10.0f, 20000.0f, hzValue)); }
void FreqShiftEngine::setHighCut (float hzValue)  { highCutHz.setTargetValue (juce::jlimit (10.0f, 20000.0f, hzValue)); }

void FreqShiftEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    for (int start = 0; start < numSamples; start += kSubBlock)
        processSubBlock (io, numChannels, start, juce::jmin (kSubBlock, numSamples - start));
}

void FreqShiftEngine::processSubBlock (float* const* io, int numChannels, int start, int n) noexcept
{
    // Per-sub-block parameter ramps: start values now, end values after n samples.
    float gainStart[kShifts], gainStep[kShifts];
    float downStart[kShifts], downStep[kShifts];
    float fbStart[kShifts], fbStep[kShifts];
    bool anyAudible = false, anyFeedback = false;

    const auto inv = 1.0f / (float) n;

    for (int k = 0; k < kShifts; ++k)
    {
        const auto i = (size_t) k;

        gainStart[k] = gain[i].getCurrentValue();
        gainStep[k] = (gain[i].skip (n) - gainStart[k]) * inv;

        downStart[k] = down[i].getCurrentValue();
        downStep[k] = (down[i].skip (n) - downStart[k]) * inv;

        fbStart[k] = feedback[i].getCurrentValue();
        fbStep[k] = (feedback[i].skip (n) - fbStart[k]) * inv;

        anyAudible = anyAudible || gainStart[k] > 0.0f || std::abs (gainStep[k]) > 0.0f;
        anyFeedback = anyFeedback || fbStart[k] > 0.0f || std::abs (fbStep[k]) > 0.0f;

        // Oscillator frequency: the value at the end of this sub-block.
        const auto f = hz[i].skip (n);
        channels[0].osc.setFrequency (k, f, sampleRate);
    }

    const auto spreadNow = spread.skip (n);
    const auto lowCut = lowCutHz.skip (n);
    const auto highCut = highCutHz.skip (n);

    if (numChannels > 1)
        for (int k = 0; k < kShifts; ++k)
            channels[1].osc.setFrequency (k, hz[(size_t) k].getCurrentValue() + kMaxSpreadHz * spreadNow, sampleRate);

    // Right channel phase lead (applied as a fixed rotation of its phasor).
    const auto spreadAngle = spreadNow * juce::MathConstants<float>::halfPi;
    const float rotCos[kMaxChannels] = { 1.0f, std::cos (spreadAngle) };
    const float rotSin[kMaxChannels] = { 0.0f, std::sin (spreadAngle) };

    if (feedbackRunning && ! anyFeedback)
        resetFeedback();
    feedbackRunning = anyFeedback;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* data = io[ch] + start;

        c.lowCut.setCutoff (lowCut);
        c.highCut.setCutoff (highCut);

        if (! anyAudible)
        {
            // Every shift off: the wet signal is silence. Keep the filters ticking.
            for (int i = 0; i < n; ++i)
                data[i] = c.highCut.process (c.lowCut.process (0.0f, rkfx::StateVariableFilter::Type::highpass),
                                             rkfx::StateVariableFilter::Type::lowpass);
            for (int i = 0; i < n; ++i)
                c.osc.advance();
            c.osc.renormalise();
            continue;
        }

        for (int i = 0; i < n; ++i)
        {
            const auto t = (float) i;

            float inI, inQ;
            c.input.process (&data[i], &inI, &inQ);

            float fbI[kShifts] = {}, fbQ[kShifts] = {};
            if (anyFeedback)
            {
                float fbIn[kShifts];
                for (int k = 0; k < kShifts; ++k)
                    fbIn[k] = (fbStart[k] + fbStep[k] * t) * rkfx::softClip (c.dc[(size_t) k].process (c.lastOut[k]));
                c.feedback.process (fbIn, fbI, fbQ);
            }

            float sum = 0.0f;
            for (int k = 0; k < kShifts; ++k)
            {
                // Single sideband: I cos + Q sin shifts down, I cos - Q sin shifts up.
                const auto cosine = c.osc.c[k] * rotCos[ch] - c.osc.s[k] * rotSin[ch];
                const auto sine   = c.osc.s[k] * rotCos[ch] + c.osc.c[k] * rotSin[ch];
                const auto sideband = 2.0f * (downStart[k] + downStep[k] * t) - 1.0f;

                const auto y = (inI + fbI[k]) * cosine + sideband * (inQ + fbQ[k]) * sine;
                c.lastOut[k] = y;
                sum += (gainStart[k] + gainStep[k] * t) * y;
            }

            c.osc.advance();

            data[i] = c.highCut.process (c.lowCut.process (sum, rkfx::StateVariableFilter::Type::highpass),
                                         rkfx::StateVariableFilter::Type::lowpass);
        }

        c.osc.renormalise();
    }
}
