#pragma once

#include "dsp/EnvelopeFollower.h"
#include "dsp/StateVariableFilter.h"
#include "safety/Smoothing.h"

#include <array>
#include <cstdint>

/** The RKVocode DSP: a zero-latency channel vocoder (mono vocoder, written
    to every output channel).

        modulator (main input) -> analysis band-passes -> envelope per band
        carrier (sidechain or internal oscillators) -> synthesis band-passes
            (shifted by the formant), each levelled out, then scaled by its
            modulator envelope -> sum
        + unvoiced (modulator highs above the top band + noise shaped by them)
        + modulator mix

    Band-passes are 4th order (two normalised 12 dB stages), spaced
    logarithmically between the low and high frequency; only the active
    band count runs. Each carrier band is divided by its own level, so the
    output follows the voice's spectrum whatever the carrier is.

    Internal carrier: band-limited (PolyBLEP) saw, pulse, noise or saw plus
    noise; a fixed note, or up to 8 MIDI voices with glide (silent when no
    notes are held). The processor feeds MIDI through noteOn/noteOff between
    calls to process(). GUI-free and allocation-free after prepare(). */
class VocodeEngine
{
public:
    static constexpr int kMaxBands = 32;
    static constexpr int kMaxVoices = 8;

    enum class Carrier { sidechain, internal };
    enum class Wave { saw, pulse, noise, sawNoise };
    enum class PitchMode { midi, fixed };

    struct Settings
    {
        int bands = 16;               // 8, 16, 24 or 32
        float lowHz = 100.0f;
        float highHz = 8000.0f;
        float formantSemitones = 0.0f;
        float attackMs = 5.0f;
        float releaseMs = 50.0f;
        float unvoiced = 0.3f;        // 0..1
        float modulatorMix = 0.0f;    // 0..1

        Carrier carrier = Carrier::internal;
        Wave wave = Wave::saw;
        PitchMode pitchMode = PitchMode::fixed;
        int fixedNote = 60;           // MIDI note (60 = C3 in Studio One naming)
        int voices = 8;
        float glideMs = 0.0f;
    };

    static double noteHz (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings&);

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;

    /** @param sidechain  carrier input for Carrier::sidechain (nullptr = silent) */
    void process (float* const* io, int numChannels, int numSamples,
                  const float* const* sidechain, int sidechainChannels) noexcept;

private:
    struct Band
    {
        std::array<rkfx::StateVariableFilter, 2> analysis, synthesis;
        rkfx::EnvelopeFollower modulatorLevel, carrierLevel;
    };

    struct Voice
    {
        int note = -1;
        bool held = false;
        double hz = 0.0, targetHz = 0.0, phase = 0.0;
        float amp = 0.0f;             // click-free on/off
        uint32_t age = 0;
    };

    double sampleRate = 48000.0;
    Settings settings;

    std::array<Band, kMaxBands> bands;
    int activeBands = 16;
    float formantRatioShown = 0.0f, lowShown = 0.0f, highShown = 0.0f;

    std::array<rkfx::StateVariableFilter, 2> unvoicedFilter, noiseFilter;
    rkfx::EnvelopeFollower unvoicedLevel;

    std::array<Voice, kMaxVoices> voices;
    uint32_t voiceClock = 0;
    double glideCoef = 0.0;
    float ampStep = 0.0f;
    uint32_t noiseSeed = 0x1234567u;

    rkfx::Smoothed unvoiced, modulatorMix, layoutFade { 1.0f };
    bool layoutPending = false;
    Settings pendingLayout;

    void configureBands();
    void applyFixedVoice() noexcept;
    float nextNoise() noexcept;
    float oscillators() noexcept;
};
