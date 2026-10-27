#pragma once

#include "dsp/Dynamics.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKCompress DSP: a clean feed-forward compressor.

        detector (input or external sidechain) -> sidechain high-pass
        -> peak or RMS level -> stereo link -> soft-knee gain computer
        -> log-domain attack/release -> gain + makeup on the signal

    No lookahead, zero latency. Sidechain listen replaces the output with the
    filtered detector signal (crossfaded). GUI-free, allocation-free. */
class CompressorEngine
{
public:
    static constexpr int kMaxChannels = 2;

    struct Settings
    {
        float thresholdDb = -18.0f;
        float ratio = 4.0f;
        float attackMs = 10.0f;
        float releaseMs = 150.0f;
        bool autoRelease = false;
        float kneeDb = 6.0f;
        rkfx::Dynamics::LevelDetector::Mode detection = rkfx::Dynamics::LevelDetector::Mode::rms;
        bool external = false;
        float sidechainHpfHz = 0.0f;   // 0 = off
        bool listen = false;
        float link = 1.0f;             // 0..1
        float makeupDb = 0.0f;
        bool autoMakeup = false;
    };

    /** Auto makeup: half the gain reduction a 0 dBFS signal would get. */
    static float autoMakeupDb (const Settings&);

    /** The makeup actually applied (manual or auto). */
    static float effectiveMakeupDb (const Settings&);

    /** Static input -> output curve in dB, including makeup (for the display). */
    static float transferDb (const Settings&, float inputDb);

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    /** @param sidechain  external detector input (nullptr = silence) */
    void process (float* const* io, int numChannels, int numSamples,
                  const float* const* sidechain, int sidechainChannels) noexcept;

    /** Most gain reduction (dB, <= 0) and loudest detector level (dB) since
        the last call; resets both. For the meter and the transfer curve dot. */
    float takeGainReductionDb() noexcept;
    float takeInputLevelDb() noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::Dynamics::LevelDetector, kMaxChannels> detectors;
    std::array<rkfx::Dynamics::GainSmoother, kMaxChannels> smoothers;
    std::array<rkfx::StateVariableFilter, kMaxChannels> hpf;
    bool hpfRunning = false;

    rkfx::Smoothed threshold { -18.0f }, ratio { 4.0f }, knee { 6.0f }, link { 1.0f }, makeup, listenMix;

    float peakReduction = 0.0f, peakInput = rkfx::Dynamics::kSilenceDb;
};
