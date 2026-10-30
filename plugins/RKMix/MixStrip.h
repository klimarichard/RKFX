#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MixDynamics.h"
#include "MixFilters.h"
#include "dsp/DelayLine.h"
#include "dsp/MatchedBiquad.h"
#include "safety/Smoothing.h"

#include <array>

/** One RKMix channel's fixed strip, in signal order:

        trim, polarity -> delay -> HPF -> gate -> EQ -> compressor

    Every section after polarity has an On switch; switching crossfades
    over 20 ms and an off section is skipped (no CPU, bit-exact). The gate
    and the compressor are RKGate's and RKCompress's (MixDynamics), the EQ
    bands the suite's matched bells and shelves (as RKParaEQ), the HPF a
    Butterworth filter (12 or 24 dB/oct). Filters are redesigned only when a
    setting moves (every 32 samples while it glides). A stereo channel runs
    one detector on the louder side. The gate and the compressor can listen
    to another input (their key) instead of the channel.

    process() calls back at each tap point (after the input stage, before
    the EQ, before the compressor, at the end), so sends can take the signal
    there. GUI-free and allocation-free after prepare(). */
class StripDsp
{
public:
    static constexpr float kMaxDelayMs = 20.0f;
    static constexpr float kGateClosedDb = MixDynamics::Gate::kClosedDb;   // the gate's Range at its bottom: fully closed
    static constexpr int kEqBands = 4;

    enum class Tap { input, preEq, preComp, preInserts };

    struct Hpf
    {
        bool on = false;
        float hz = 80.0f;
        bool steep = false;                // 24 dB/oct (else 12)
    };

    struct Gate
    {
        bool on = false;
        float thresholdDb = -50.0f, rangeDb = kGateClosedDb;
        float attackMs = 1.0f, holdMs = 50.0f, releaseMs = 150.0f;
        int key = 0;                       // 0 = itself, n = input n
    };

    struct EqBand
    {
        bool bell = true;                  // Low and High: shelf or bell; the mids are bells
        float hz = 1000.0f, gainDb = 0.0f, q = 1.0f;
    };

    struct Eq
    {
        Eq()
        {
            bands[0] = { false, 100.0f, 0.0f, 0.7f };
            bands[1] = { true, 250.0f, 0.0f, 1.0f };
            bands[2] = { true, 2500.0f, 0.0f, 1.0f };
            bands[3] = { false, 8000.0f, 0.0f, 0.7f };
        }
        bool on = false;
        std::array<EqBand, kEqBands> bands;
    };

    struct Comp
    {
        bool on = false;
        float thresholdDb = -20.0f, ratio = 3.0f, kneeDb = 6.0f;
        float attackMs = 10.0f, releaseMs = 150.0f, makeupDb = 0.0f;
        int key = 0;
    };

    struct Settings
    {
        float trimDb = 0.0f;
        std::array<bool, 2> polarity {};
        bool delayOn = false;
        float delayMs = 0.0f;
        Hpf hpf;
        Gate gate;
        Eq eq;
        Comp comp;
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** @param snap  jump to the new values (no glides): after a reset */
    void setSettings (const Settings&, bool snap);
    const Settings& getSettings() const noexcept { return settings; }

    /** Runs the strip: @p source (the channel's inputs) into @p io.
        @param scratch  2 channels of at least the block size, for crossfades
        @param gateKey, compKey  the keys' signals (nullptr = the channel itself)
        @param onTap    called as onTap (Tap, io) at each tap point */
    template <typename OnTap>
    void process (const float* const* source, float* const* io, int numSides, int numSamples,
                  juce::AudioBuffer<float>& scratch, const float* gateKey, const float* compKey, OnTap&& onTap) noexcept
    {
        processInput (source, io, numSides, numSamples, scratch);
        onTap (Tap::input, io);
        processHpfAndGate (io, numSides, numSamples, scratch, gateKey);
        onTap (Tap::preEq, io);
        processEq (io, numSides, numSamples, scratch);
        onTap (Tap::preComp, io);
        processComp (io, numSides, numSamples, scratch, compKey);
        onTap (Tap::preInserts, io);
    }

    /** Most gain reduction since the last call (dB, <= 0); resets. */
    float takeGateReductionDb() noexcept { return gate.takeGainReductionDb(); }
    float takeCompReductionDb() noexcept { return comp.takeGainReductionDb(); }

    /** The HPF and EQ response at @p hz (for the displays). */
    static double responseDb (const Settings&, double hz, double sampleRate);

    /** The compressor's static curve including makeup (for the display). */
    static float compTransferDb (const Comp&, float inputDb);

private:
    using SectionFade = MixFilters::SectionFade;
    using Biquad = MixFilters::Biquad;
    using HighPass = MixFilters::CutFilter;

    struct BandState
    {
        rkfx::Smoothed logHz, gainDb, q;
        Biquad filter;
        bool active = false;               // not flat: processed
    };

    double sampleRate = 48000.0;
    Settings settings;

    std::array<rkfx::Smoothed, 2> input;
    SectionFade delayFade, hpfFade, gateFade, eqFade, compFade;
    rkfx::Smoothed delaySamples;
    std::array<rkfx::DelayLine, 2> delay;

    // The HPF: a slope change crossfades from the active filter to the other.
    std::array<HighPass, 2> highPass;
    int activeHighPass = 0;
    rkfx::Smoothed slopeFade;
    rkfx::Smoothed hpfLogHz;

    MixDynamics::Gate gate;
    MixDynamics::Compressor comp;
    std::array<BandState, kEqBands> bands;

    void processInput (const float* const* source, float* const* io, int numSides, int numSamples, juce::AudioBuffer<float>&) noexcept;
    void processHpfAndGate (float* const* io, int numSides, int numSamples, juce::AudioBuffer<float>&, const float* key) noexcept;
    void processEq (float* const* io, int numSides, int numSamples, juce::AudioBuffer<float>&) noexcept;
    void processComp (float* const* io, int numSides, int numSamples, juce::AudioBuffer<float>&, const float* key) noexcept;

    void updateBand (int band) noexcept;
    void designHighPass (HighPass&, double hz, int stages) const noexcept;
    void startSlopeChange() noexcept;
    static rkfx::BiquadCoefficients designBand (int band, bool bell, double hz, double gainDb, double q, double sampleRate);
};
