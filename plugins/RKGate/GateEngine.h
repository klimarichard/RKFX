#pragma once

#include "dsp/Dynamics.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKGate DSP: a gate / downward expander.

        detector (input or external sidechain) -> key high-pass and low-pass
        -> fast peak envelope -> gate (with hysteresis) or expander curve
        -> hold -> log-domain attack/release -> gain on the signal

    Shares the sidechain bus, level conversion and log-domain gain smoothing
    with RKCompress. No lookahead, zero latency: at the shortest attack a
    very sharp transient can lose a fraction of its first millisecond.

    GUI-free and allocation-free after prepare(). */
class GateEngine
{
public:
    static constexpr int kMaxChannels = 2;

    /** Range at or below this means fully closed (silence). */
    static constexpr float kClosedDb = -100.0f;
    static constexpr float kFloorDb = -120.0f;

    enum class Mode { gate, expander };

    struct Settings
    {
        Mode mode = Mode::gate;
        float thresholdDb = -50.0f;
        float ratio = 2.0f;             // expander 1:ratio
        float rangeDb = kClosedDb;      // 0..-80, or kClosedDb = fully closed
        float attackMs = 0.5f;
        float holdMs = 50.0f;
        float releaseMs = 100.0f;
        float hysteresisDb = 3.0f;
        bool external = false;
        float keyHighPassHz = 0.0f;     // 0 = off
        float keyLowPassHz = 0.0f;      // 0 = off
        bool listen = false;
        bool link = true;
    };

    /** The gain change (dB, <= 0) the gate or expander asks for, before
        timing, for a detector level. @p open is the gate's state, updated. */
    static float targetGainDb (const Settings&, float levelDb, bool& open);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples,
                  const float* const* sidechain, int sidechainChannels) noexcept;

    /** Most gain reduction since the last call (dB, <= 0); resets. */
    float takeGainReductionDb() noexcept;

private:
    struct Channel
    {
        rkfx::EnvelopeFollower envelope;
        rkfx::StateVariableFilter highPass, lowPass;
        rkfx::Dynamics::GainSmoother smoother;
        bool open = false;
        int holdRemaining = 0;
    };

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Channel, kMaxChannels> channels;
    bool highPassRunning = false, lowPassRunning = false;
    int holdSamples = 0;

    rkfx::Smoothed listenMix;
    float peakReduction = 0.0f;

    float gateChannel (Channel&, float levelDb) noexcept;
};
