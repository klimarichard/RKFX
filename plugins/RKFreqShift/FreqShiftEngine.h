#pragma once

#include "dsp/HilbertTransformer.h"
#include "dsp/QuadratureOscillator.h"
#include "dsp/StateVariableFilter.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKFreqShift DSP: a shared Hilbert transformer feeding four
    single-sideband shifters, each with its own quadrature oscillator, up/down
    mix and feedback, summed and filtered (wet low cut and high cut).

    Feedback: each shifter feeds its own output (DC blocked, soft clipped) back
    to its input. The Hilbert transform is linear, so the input's I/Q comes
    from the shared network and the feedback adds its own I/Q from a second
    network, run for all four shifters in lock-step and skipped entirely while
    every feedback is at 0.

    GUI-free and allocation-free after prepare(). Up to two channels; with two,
    the right channel's oscillators are offset by the stereo spread. */
class FreqShiftEngine
{
public:
    static constexpr int kShifts = 4;
    static constexpr int kMaxChannels = 2;

    /** Coefficients follow the smoothed parameters every kSubBlock samples. */
    static constexpr int kSubBlock = 32;

    /** Right channel oscillators: up to this much higher, and up to 90 degrees ahead. */
    static constexpr float kMaxSpreadHz = 5.0f;

    struct ShiftSettings
    {
        bool on = false;
        float hz = 100.0f;
        float down = 0.0f;       // 0 = all up, 1 = all down
        float feedback = 0.0f;   // 0..0.9
        float level = 1.0f;      // 0..1
    };

    void prepare (double sampleRate);
    void reset();

    void setShift (int index, const ShiftSettings&);
    void setSpread (float spread01);
    void setLowCut (float hz);
    void setHighCut (float hz);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    struct Channel
    {
        rkfx::HilbertTransformer<1> input;
        rkfx::HilbertTransformer<kShifts> feedback;
        rkfx::QuadratureOscillator<kShifts> osc;
        std::array<rkfx::DCBlocker, kShifts> dc;
        float lastOut[kShifts] {};
        rkfx::StateVariableFilter lowCut, highCut;
    };

    double sampleRate = 48000.0;
    std::array<Channel, kMaxChannels> channels;

    std::array<rkfx::Smoothed, kShifts> hz, down, feedback, gain;
    rkfx::Smoothed spread;
    rkfx::SmoothedGain lowCutHz { 20.0f }, highCutHz { 20000.0f };
    bool feedbackRunning = false;

    void resetFeedback();
    void processSubBlock (float* const* io, int numChannels, int start, int n) noexcept;
};
