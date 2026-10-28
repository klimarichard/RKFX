#pragma once

#include "dsp/EnvelopeFollower.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKTransient DSP (wet signal only; the base processor does dry/wet).

    Level-independent transient shaping, with no threshold. The detector
    (input through the focus filters) feeds two pairs of power averages:

    - attack: a 3 ms average against one over the attack time; where the
      fast one is above, the sound is in its attack;
    - sustain: a 15 ms average against one over the sustain time; where the
      slow one is above, the note is ringing out.

    On a steady sound both averages of a pair settle on the same value, so
    steady tones are left alone.

    The gain is the dB difference times the amount (boost or cut), so the
    result depends only on the shape of the sound, not its level. Gain
    changes are smoothed (about 4 ms) so low notes don't distort.
    GUI-free and allocation-free after prepare(). */
class TransientEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr float kMaxGainDb = 24.0f;

    struct Settings
    {
        float attack = 0.0f;           // -1..1
        float sustain = 0.0f;          // -1..1
        float attackTimeMs = 10.0f;
        float sustainTimeMs = 100.0f;
        float focusHighPassHz = 0.0f;  // 0 = off
        float focusLowPassHz = 0.0f;   // 0 = off
        bool listen = false;
        bool link = true;
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    struct Detector
    {
        rkfx::EnvelopeFollower attackFast, attackSlow, sustainFast, sustainSlow;
        float smoothedGainDb = 0.0f;
    };

    double sampleRate = 48000.0;
    Settings settings;

    std::array<Detector, kMaxChannels> detectors;
    std::array<rkfx::StateVariableFilter, kMaxChannels> highPass, lowPass;
    bool highPassRunning = false, lowPassRunning = false;

    float gainSmoothing = 0.0f;
    rkfx::Smoothed attack, sustain, listen;

    void configureFollowers();
    float gainDb (Detector&, float detectorLevel, float attackAmount, float sustainAmount) noexcept;
};
