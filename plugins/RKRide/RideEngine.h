#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Dynamics.h"
#include "dsp/MatchedBiquad.h"
#include "dsp/StateVariableFilter.h"

#include <array>

/** The RKRide DSP: an automatic fader rider.

        input -> weighting (HP 80 Hz + 4 dB shelf at 2 kHz) -> RMS (200 ms)
        -> wanted gain = Target - level, within Max Boost / Max Cut
        -> glide in dB (Speed up, Speed / 2 down) -> gain on the signal

    Below the Floor the gain holds; after Return seconds below it, it drifts
    back to 0 dB. In Relative mode the target is the sidechain's level plus
    Above Music (the absolute Target while the sidechain is silent).

    No lookahead, zero latency. GUI-free, allocation-free. */
class RideEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kUpdateInterval = 16;          // samples between gain computer updates
    static constexpr float kWindowMs = 200.0f;          // RMS averaging
    static constexpr float kReturnGlideMs = 1000.0f;    // drift back to 0 dB
    static constexpr float kSidechainSilentDb = -60.0f;

    struct Settings
    {
        float targetDb = -18.0f;
        float maxBoostDb = 6.0f;
        float maxCutDb = 6.0f;
        float speedMs = 500.0f;
        float floorDb = -50.0f;
        float returnMs = 3000.0f;
        bool relative = false;
        float aboveDb = 3.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    /** @param sidechain  the music for Relative mode (nullptr = none) */
    void process (float* const* io, int numChannels, int numSamples,
                  const float* const* sidechain, int sidechainChannels) noexcept;

    struct Meter
    {
        float levelDb = rkfx::Dynamics::kSilenceDb;   // loudest weighted level since the last call
        float targetDb = -18.0f;                      // the target in use (follows the music in Relative mode)
        float gainDb = 0.0f;                          // the gain right now
        bool holding = false;                         // below the Floor
    };

    /** The meter since the last call (resets the level). */
    Meter takeMeter() noexcept;

    float getGainDb() const noexcept { return gainDb; }

private:
    /** Weighting and RMS for one signal (the input or the sidechain). */
    struct Detector
    {
        std::array<rkfx::StateVariableFilter, kMaxChannels> highPass;
        rkfx::Biquad shelf;   // both channels
        float meanSquare = 0.0f;

        void prepare (double sampleRate);
        void reset();
        /** Adds one sample frame; returns nothing (read levelDb()). */
        void process (const float* const* x, int numChannels, int index, float coefficient) noexcept;
        float levelDb() const noexcept;
    };

    double sampleRate = 48000.0;
    Settings settings;

    Detector input, music;
    float windowCoefficient = 0.0f;

    float gainDb = 0.0f, wantedDb = 0.0f, glideCoefficient = 0.0f;
    float silentMs = 0.0f;
    int countdown = 0;
    Meter meter;

    void updateGainComputer (bool hasSidechain) noexcept;
};
