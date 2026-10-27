#pragma once

#include "dsp/EnvelopeFollower.h"
#include "dsp/LFO.h"
#include "dsp/StateVariableFilter.h"
#include "dsp/TempoSync.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKFilter DSP: a multimode zero-delay-feedback filter whose cutoff is
    modulated by an envelope follower (auto-wah) and an LFO.

    Slopes: 12 dB/oct per stage, up to four stages. Only the first stage
    carries resonance; the extra stages are flat Butterworth sections, so the
    resonance sounds the same at every slope. Band-pass stages each add 6
    dB/oct per side and are normalised to 0 dB at the centre.

    Changing mode or slope crossfades between two filter banks over 10 ms, so
    it never clicks. GUI-free and allocation-free after prepare(). */
class FilterEngine
{
public:
    enum class Mode { lowpass, highpass, bandpass };

    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxStages = 4;
    static constexpr float kMaxQ = 10.0f;            // resonance 100%
    static constexpr float kModOctaves = 4.0f;       // envelope / LFO at 100%
    static constexpr float kEnvFloorDb = -48.0f;     // envelope range: -48 dB (closed) to 0 dB (fully open)

    struct Settings
    {
        Mode mode = Mode::lowpass;
        int stages = 1;                  // 1..4 (12 to 48 dB/oct)
        float cutoffHz = 1000.0f;
        float resonance = 0.2f;          // 0..1

        float envAmount = 0.0f;          // -1..1
        float envSensDb = 0.0f;
        float envAttackMs = 5.0f;
        float envReleaseMs = 150.0f;

        float lfoDepth = 0.0f;           // -1..1
        rkfx::LFO::Shape lfoShape = rkfx::LFO::Shape::sine;
        float lfoRateHz = 1.0f;
        bool lfoSync = false;
        int lfoDivision = rkfx::SyncDivision::index (4);
        float lfoPhaseDeg = 0.0f;        // right channel offset
    };

    /** Resonance (0..1) to the resonant stage's Q: 0.707 (no peak) to kMaxQ. */
    static float resonanceToQ (float resonance01) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples, const rkfx::Transport&) noexcept;

private:
    struct Bank
    {
        Mode mode = Mode::lowpass;
        int stages = 1;
        float extraK[kMaxStages - 1] {};   // 1/Q of the Butterworth stages
        std::array<std::array<rkfx::StateVariableFilter, kMaxStages>, kMaxChannels> filters;

        void configure (Mode, int stages);
        void reset();
        float process (int channel, float x, float g, float resonantK) noexcept;
    };

    double sampleRate = 48000.0;
    Settings settings;

    std::array<Bank, 2> banks;
    int active = 0;
    int fadeRemaining = 0, fadeLength = 480;

    rkfx::EnvelopeFollower envelope;
    std::array<rkfx::LFO, kMaxChannels> lfos;

    rkfx::Smoothed cutoffOctaves { 0.0f }, resonance { 0.2f }, envAmount, lfoDepth;
    rkfx::SmoothedGain envSens { 1.0f };
};
