#include "FilterEngine.h"

#include <cmath>

namespace
{
    using Type = rkfx::StateVariableFilter::Type;

    constexpr float kButterworthQ = 0.70710678f;

    /** Butterworth section Qs for the extra stages (orders 2, 4 and 6). */
    constexpr float kExtraQ[3][3] = {
        { 0.70710678f, 0.0f,        0.0f },
        { 0.54119610f, 1.30656296f, 0.0f },
        { 0.51763809f, 0.70710678f, 1.93185165f }
    };
}

float FilterEngine::resonanceToQ (float r) noexcept
{
    return kButterworthQ * std::pow (kMaxQ / kButterworthQ, juce::jlimit (0.0f, 1.0f, r));
}

//==============================================================================
void FilterEngine::Bank::configure (Mode newMode, int newStages)
{
    mode = newMode;
    stages = juce::jlimit (1, kMaxStages, newStages);

    for (int s = 0; s < stages - 1; ++s)
        extraK[s] = 1.0f / (mode == Mode::bandpass ? kButterworthQ : kExtraQ[stages - 2][s]);
}

void FilterEngine::Bank::reset()
{
    for (auto& channel : filters)
        for (auto& f : channel)
            f.reset();
}

float FilterEngine::Bank::process (int channel, float x, float g, float resonantK) noexcept
{
    auto& f = filters[(size_t) channel];

    f[0].setCoefficients (g, resonantK);
    for (int s = 1; s < stages; ++s)
        f[(size_t) s].setCoefficients (g, extraK[s - 1]);

    switch (mode)
    {
        case Mode::lowpass:
            for (int s = 0; s < stages; ++s)
                x = f[(size_t) s].process (x, Type::lowpass);
            return x;

        case Mode::highpass:
            for (int s = 0; s < stages; ++s)
                x = f[(size_t) s].process (x, Type::highpass);
            return x;

        case Mode::bandpass:
            for (int s = 0; s < stages; ++s)
                x = f[(size_t) s].processNormalisedBandpass (x);
            return x;
    }

    return x;
}

//==============================================================================
void FilterEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fadeLength = juce::jmax (1, (int) (0.01 * sampleRate));

    envelope.prepare (sampleRate);
    for (auto& lfo : lfos)
        lfo.prepare (sampleRate);

    cutoffOctaves.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    resonance.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    envAmount.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    lfoDepth.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    envSens.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    reset();
}

void FilterEngine::reset()
{
    cutoffOctaves.setCurrentAndTargetValue (cutoffOctaves.getTargetValue());
    resonance.setCurrentAndTargetValue (resonance.getTargetValue());
    envAmount.setCurrentAndTargetValue (envAmount.getTargetValue());
    lfoDepth.setCurrentAndTargetValue (lfoDepth.getTargetValue());
    envSens.setCurrentAndTargetValue (envSens.getTargetValue());

    active = 0;
    fadeRemaining = 0;
    banks[0].configure (settings.mode, settings.stages);
    for (auto& b : banks)
        b.reset();

    envelope.reset();
    lfos[0].reset();
    lfos[1].reset (settings.lfoPhaseDeg / 360.0);
}

void FilterEngine::setSettings (const Settings& s)
{
    settings = s;

    cutoffOctaves.setTargetValue (std::log2 (juce::jlimit (20.0f, 20000.0f, s.cutoffHz)));
    resonance.setTargetValue (juce::jlimit (0.0f, 1.0f, s.resonance));
    envAmount.setTargetValue (juce::jlimit (-1.0f, 1.0f, s.envAmount));
    lfoDepth.setTargetValue (juce::jlimit (-1.0f, 1.0f, s.lfoDepth));
    envSens.setTargetValue (juce::Decibels::decibelsToGain (s.envSensDb));

    envelope.setTimes (s.envAttackMs, s.envReleaseMs);
    for (auto& lfo : lfos)
        lfo.setShape (s.lfoShape);
}

void FilterEngine::process (float* const* io, int numChannels, int numSamples, const rkfx::Transport& transport) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    // A mode or slope change starts a crossfade to the other bank (one at a time).
    if (fadeRemaining == 0 && (settings.mode != banks[(size_t) active].mode || settings.stages != banks[(size_t) active].stages))
    {
        auto& next = banks[(size_t) (1 - active)];
        next.configure (settings.mode, settings.stages);
        next.reset();
        fadeRemaining = fadeLength;
    }

    // LFO: free-running, or locked to the host's beat when it provides one.
    const auto phaseOffset = settings.lfoPhaseDeg / 360.0;
    if (settings.lfoSync)
    {
        const auto beats = rkfx::SyncDivision::beats (settings.lfoDivision);
        for (auto& lfo : lfos)
            lfo.setRate (transport.bpm / 60.0 / beats);

        if (transport.hasPosition && transport.playing)
            lfos[0].syncTo (transport.ppq, beats);
    }
    else
    {
        for (auto& lfo : lfos)
            lfo.setRate (settings.lfoRateHz);
    }
    lfos[1].syncTo (lfos[0].getPhase(), 1.0, phaseOffset);

    const auto maxOctave = std::log2 (juce::jmin (20000.0, 0.45 * sampleRate));
    const auto minOctave = std::log2 (20.0);

    const auto modulating = std::abs (envAmount.getTargetValue()) > 0.0f || envAmount.isSmoothing()
                         || std::abs (lfoDepth.getTargetValue()) > 0.0f || lfoDepth.isSmoothing();

    // Without modulation or a cutoff glide, the coefficient is the same for the whole block.
    const auto staticCutoff = ! modulating && ! cutoffOctaves.isSmoothing();
    const auto staticG = rkfx::StateVariableFilter::gFor (
        std::exp2 ((double) juce::jlimit ((float) minOctave, (float) maxOctave, cutoffOctaves.getTargetValue())), sampleRate);

    auto* const* data = io;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto baseOctave = cutoffOctaves.getNextValue();
        const auto resonantK = 1.0f / resonanceToQ (resonance.getNextValue());
        const auto env = envAmount.getNextValue();
        const auto depth = lfoDepth.getNextValue();
        const auto sens = envSens.getNextValue();

        // Stereo-linked envelope: the louder channel drives both.
        float envelope01 = 0.0f;
        {
            float detector = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                detector = juce::jmax (detector, std::abs (data[ch][i]));

            const auto level = envelope.process (detector * sens);
            if (std::abs (env) > 0.0f)
            {
                const auto db = 20.0f * std::log10 (level + 1.0e-9f);
                envelope01 = juce::jlimit (0.0f, 1.0f, (db - kEnvFloorDb) / -kEnvFloorDb);
            }
        }

        const auto fadeIn = fadeRemaining > 0 ? 1.0f - (float) fadeRemaining / (float) fadeLength : 1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto lfoValue = lfos[(size_t) ch].next();

            auto g = staticG;
            if (! staticCutoff)
            {
                auto octave = baseOctave + kModOctaves * (env * envelope01 + depth * lfoValue);
                octave = juce::jlimit ((float) minOctave, (float) maxOctave, octave);
                g = rkfx::StateVariableFilter::gFor (std::exp2 ((double) octave), sampleRate);
            }

            const auto x = data[ch][i];
            auto y = banks[(size_t) active].process (ch, x, g, resonantK);

            if (fadeRemaining > 0)
                y += fadeIn * (banks[(size_t) (1 - active)].process (ch, x, g, resonantK) - y);

            data[ch][i] = y;
        }

        if (numChannels == 1)
            lfos[1].next();   // keep the right LFO in step for a later switch to stereo

        if (fadeRemaining > 0 && --fadeRemaining == 0)
            active = 1 - active;
    }
}
