#pragma once

#include "dsp/DelayLine.h"
#include "dsp/TempoSync.h"
#include "dsp/Tilt.h"
#include "safety/Smoothing.h"

#include <array>

/** The RKChorus DSP (wet signal only; the base processor does dry/wet).

    One delay line per channel, read by 1 to 4 voices. Each voice's delay is
    modulated by a sine LFO; the voices' phases are spread evenly, and the
    right channel runs opposite to the left, so a mono input becomes a wide
    stereo chorus. Then tone tilt and stereo width.

    The wet level is scaled by 1/sqrt(voices), so changing the voice count
    keeps the loudness steady; the change crossfades over 20 ms.
    GUI-free and allocation-free after prepare(). */
class ChorusEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxVoices = 4;
    static constexpr float kMaxDelayMs = 30.0f;
    /** Modulation at 100% depth, as a fraction of the delay (stays positive). */
    static constexpr float kMaxDepthFraction = 0.45f;

    struct Settings
    {
        int voices = 1;
        float rateHz = 0.8f;
        float depth = 0.4f;         // 0..1
        float delayMs = 12.0f;
        float width = 1.0f;         // 0..1
        float tone = 0.0f;          // -1..1
        bool sync = false;
        int division = rkfx::SyncDivision::index (4);
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    /** In place; with two channels the output is stereo even for identical inputs. */
    void process (float* const* io, int numChannels, int numSamples, const rkfx::Transport&) noexcept;

private:
    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::DelayLine, kMaxChannels> lines;
    std::array<rkfx::Tilt, kMaxChannels> tilts;

    double phase = 0.0;   // LFO phase, 0..1
    rkfx::Smoothed delaySamples { 576.0f }, depth { 0.4f }, width { 1.0f };

    int voices = 1, previousVoices = 1;
    int fadeRemaining = 0, fadeLength = 960;

    float voicesOutput (int channel, int count, float delay, float modulation) const noexcept;
};
