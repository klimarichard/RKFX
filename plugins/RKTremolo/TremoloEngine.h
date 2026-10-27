#pragma once

#include "dsp/Crossover.h"
#include "dsp/LFO.h"
#include "dsp/TempoSync.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKTremolo DSP: a tremolo and auto-panner with a harmonic mode.

    - Classic: the volume follows the LFO, between full level and
      (1 - depth). With the right channel's LFO offset by 180 degrees the
      volume moves from side to side: an auto-panner.
    - Harmonic (brownface style): a Linkwitz-Riley crossover splits lows and
      highs, whose volumes move in opposite directions, then they are summed.
      The crossover is computed only in this mode.

    The gain curve is always slewed (at least about 1 ms, more with the
    smoothing control), so even a square wave at 100% depth never clicks.
    GUI-free and allocation-free after prepare(). */
class TremoloEngine
{
public:
    static constexpr int kMaxChannels = 2;

    enum class Mode { classic, harmonic };

    struct Settings
    {
        Mode mode = Mode::classic;
        float depth = 0.5f;              // 0..1
        float rateHz = 4.0f;
        bool sync = false;
        int division = rkfx::SyncDivision::index (8);
        rkfx::LFO::Shape shape = rkfx::LFO::Shape::sine;
        float smooth = 0.3f;             // 0..1
        float phaseDeg = 0.0f;           // right channel offset
        float crossoverHz = 600.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples, const rkfx::Transport&) noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::LFO, kMaxChannels> lfos;
    std::array<rkfx::Crossover, kMaxChannels> crossovers;
    std::array<float, kMaxChannels> slewed {};    // smoothed LFO value per channel

    rkfx::Smoothed depth { 0.5f }, harmonicMix;
    bool crossoverRunning = false;

    float slewCoefficient (double cycleHz) const noexcept;
};
