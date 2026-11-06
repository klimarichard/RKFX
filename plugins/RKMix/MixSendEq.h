#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MixFilters.h"
#include "dsp/MatchedBiquad.h"
#include "safety/Smoothing.h"

#include <array>

/** The small EQ on one channel's send to one Aux or FX bus (docs/design/
    RKMix Send EQ.md): a low cut (12 dB/oct), a low shelf, a mid bell and a
    high shelf, the strip EQ's matched filters. Zero latency.

    On switches with a 20 ms crossfade (the filters start clean); an off EQ
    is skipped entirely, a band at 0 dB is skipped, and the low cut at Off
    is skipped (with its own crossfade in and out). Gains, frequencies and
    Q glide; the filters are redesigned every 32 samples while they do.
    A stereo pair runs both sides through the same settings.
    GUI-free and allocation-free after prepare(). */
class SendEqDsp
{
public:
    static constexpr int kBands = 3;     // low shelf, mid bell, high shelf

    struct Settings
    {
        bool on = false;
        float lowCutHz = 0.0f;           // 0 = Off
        float lowHz = 150.0f, lowDb = 0.0f;
        float midHz = 1000.0f, midDb = 0.0f, midQ = 1.0f;
        float highHz = 8000.0f, highDb = 0.0f;

        /** Nothing would change the sound: Off, or no cut and every band at 0 dB. */
        bool isFlat() const noexcept
        {
            return ! on || (lowCutHz <= 0.0f && juce::exactlyEqual (lowDb, 0.0f)
                            && juce::exactlyEqual (midDb, 0.0f) && juce::exactlyEqual (highDb, 0.0f));
        }
    };

    void prepare (double sampleRate);
    void reset();

    /** @param snap  jump to the new values (no glides, no crossfade) */
    void setSettings (const Settings&, bool snap) noexcept;
    const Settings& getSettings() const noexcept { return settings; }

    /** True when the EQ is fully off (not even fading out): the send takes the plain path. */
    bool isOff() const noexcept { return onFade.isOff(); }

    /** Filters @p io in place. @param scratch  2 channels of at least the block size (the crossfade) */
    void process (float* const* io, int numSides, int numSamples, juce::AudioBuffer<float>& scratch) noexcept;

    /** The response at @p hz in dB (for the displays): 0 when off. */
    static double responseDb (const Settings&, double hz, double sampleRate);

private:
    using Biquad = MixFilters::Biquad;

    struct Band
    {
        rkfx::Smoothed logHz, gainDb, q;
        Biquad filter;
        bool active = false;             // not flat: processed
    };

    double sampleRate = 48000.0;
    Settings settings;
    MixFilters::SectionFade onFade, lowCutFade;
    rkfx::Smoothed lowCutLogHz;
    MixFilters::CutFilter lowCut;
    std::array<Band, kBands> bands;

    void updateBand (int band) noexcept;
    void updateLowCut() noexcept;
    void clearFilters() noexcept;
    static rkfx::BiquadCoefficients designBand (int band, double hz, double gainDb, double q, double sampleRate);
};
