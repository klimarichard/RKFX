#pragma once

#include "dsp/EnvelopeFollower.h"
#include "dsp/LFO.h"
#include "dsp/TempoSync.h"
#include "safety/DCBlocker.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKPhase DSP (wet signal only; the base processor mixes it with dry,
    which is where the phaser's notches come from).

    Phaser: a chain of 2 to 12 first-order allpass filters, all at the same
    frequency, swept around the centre by an LFO and an envelope follower
    (their movements add up), with feedback through a DC blocker and soft
    clipper. Only the active stages run; changing the stage count crossfades
    between the two outputs over 10 ms.

    Uni-Vibe: four allpass stages at staggered frequencies, in the ratios of
    the original's capacitors (15 nF, 220 nF, 470 pF, 4.7 nF; Center sets
    the 4.7 nF stage), without feedback. The LFO drives a lamp: its light
    is curved (squared) and a photocell follows it quickly as it brightens
    and slowly as it dims, which gives the vibe's lopsided "throb". Dry/Wet
    50% is the chorus setting, 100% the vibrato.

    Switching the mode dips the wet signal for about 10 ms. The sweep is
    clamped between 20 Hz and just below Nyquist. GUI-free and
    allocation-free after prepare(). */
class PhaseEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxStages = 12;
    static constexpr float kLfoOctaves = 2.0f;   // either side of the centre at 100% depth
    static constexpr float kEnvOctaves = 4.0f;   // envelope at +/-100%

    struct Settings
    {
        int stages = 4;
        float centreHz = 800.0f;
        float feedback = 0.3f;          // -0.95..0.95
        float rateHz = 0.5f;
        float depth = 0.7f;             // 0..1
        bool sync = false;
        int division = rkfx::SyncDivision::index (2);
        float phaseDeg = 90.0f;
        float envAmount = 0.0f;         // -1..1
        float envSensDb = 0.0f;
        float envAttackMs = 5.0f;
        float envReleaseMs = 150.0f;
        bool univibe = false;
    };

    static constexpr int kVibeStages = 4;
    /** Uni-Vibe stage frequencies relative to Center (from the capacitors). */
    static constexpr float vibeRatios[kVibeStages] = { 4.7f / 15.0f, 4.7f / 220.0f, 4.7f / 0.47f, 1.0f };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples, const rkfx::Transport&) noexcept;

private:
    struct Channel
    {
        std::array<float, kMaxStages> x1 {}, y1 {};   // allpass states
        rkfx::DCBlocker dc;
        float lastOut = 0.0f;
        float cell = 0.5f;                                   // Uni-Vibe photocell, 0..1
        std::array<float, kVibeStages> vibeCoefs {};
    };

    double sampleRate = 48000.0;
    Settings settings;

    std::array<Channel, kMaxChannels> channels;
    std::array<rkfx::LFO, kMaxChannels> lfos;
    rkfx::EnvelopeFollower envelope;

    int stages = 4, previousStages = 4;
    int fadeRemaining = 0, fadeLength = 480;

    bool univibe = false;
    rkfx::Smoothed modeFade { 1.0f };
    float cellBrighter = 0.0f, cellDimmer = 0.0f;
    int untilVibeUpdate = 0;

    void clearStates() noexcept;

    rkfx::Smoothed centreOctave { 9.64f }, depth { 0.7f }, feedback { 0.3f }, envAmount;
    rkfx::SmoothedGain envSens { 1.0f };
};
