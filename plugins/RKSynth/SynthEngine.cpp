#include "SynthEngine.h"

#include "dsp/Dynamics.h"
#include "safety/SoftClip.h"

#include <cmath>

namespace
{
    constexpr float kGateAttackMs = 1.0f;
    constexpr float kLevelAttackMs = 2.0f, kLevelReleaseMs = 60.0f;
    constexpr float kOnsetFastMs = 3.0f, kOnsetSlowMs = 60.0f, kOnsetJumpDb = 6.0f;
    constexpr double kAmpAttackMs = 3.0, kAmpReleaseMs = 10.0, kEnvAttackMs = 2.0;

    float coefficient (double ms, double rate) { return ms <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (0.001 * ms * rate)); }
    float powerDb (float p) noexcept { return 10.0f * std::log10 (p + 1.0e-12f); }
}

void SynthEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    tracker.prepare (sampleRate);
    filter1.prepare (sampleRate);
    filter2.prepare (sampleRate);

    gateLevel.prepare (sampleRate);
    inputLevel.prepare (sampleRate);
    inputLevel.setTimes (kLevelAttackMs, kLevelReleaseMs);
    onsetFast.prepare (sampleRate);
    onsetFast.setTimes (kOnsetFastMs, kOnsetFastMs);
    onsetSlow.prepare (sampleRate);
    onsetSlow.setTimes (kOnsetSlowMs, kOnsetSlowMs);

    ampAttack = coefficient (kAmpAttackMs, sampleRate);
    ampRelease = coefficient (kAmpReleaseMs, sampleRate);
    envAttack = coefficient (kEnvAttackMs, sampleRate);

    for (auto* s : { &cutoff, &resonance, &envAmount, &sub })
        s->reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

float SynthEngine::gateReleaseMs (Range r) noexcept
{
    // About 2.5 periods of the range's lowest note: the level ripples less
    // than the 3 dB hysteresis even on a bass's slow cycles, yet the synth
    // stops soon after the note.
    return juce::jmax (10.0f, 2500.0f / rkfx::PitchTracker::limitsFor (r).lowHz);
}

void SynthEngine::reset()
{
    tracker.setRange (settings.range);
    gateLevel.setTimes (kGateAttackMs, gateReleaseMs (settings.range));
    tracker.reset();
    filter1.reset();
    filter2.reset();
    for (auto* f : { &gateLevel, &inputLevel, &onsetFast, &onsetSlow })
        f->reset();

    for (auto* s : { &cutoff, &resonance, &envAmount, &sub })
        s->setCurrentAndTargetValue (s->getTargetValue());

    gateOpen = false;
    fresh = armed = true;
    phase = subPhase = 0.0;
    amp = env = envSmooth = 0.0f;
    updateFilter();
}

void SynthEngine::setSettings (const Settings& s)
{
    if (s.range != settings.range)
    {
        tracker.setRange (s.range);
        gateLevel.setTimes (kGateAttackMs, gateReleaseMs (s.range));
        fresh = true;
    }
    settings = s;

    cutoff.setTargetValue (s.cutoffHz);
    resonance.setTargetValue (s.resonance);
    envAmount.setTargetValue (s.envAmount);
    sub.setTargetValue (s.sub);
    envDecay = coefficient (juce::jmax (1.0f, s.decayMs) / 4.6f, sampleRate);   // -40 dB after the decay time
    glideCoef = coefficient (s.glideMs / 3.0f, sampleRate);   // 95% of the way in the glide time
}

void SynthEngine::updateFilter() noexcept
{
    const auto hz = cutoff.getNextValue() * std::exp2 (kMaxEnvOctaves * envAmount.getNextValue() * envSmooth);
    const auto q = 0.7f + 7.0f * resonance.getNextValue();
    for (auto* s : { &cutoff, &envAmount, &resonance })
        s->skip (kUpdateInterval - 1);

    const auto clamped = juce::jmin (hz, 0.45f * (float) sampleRate);
    filter1.setCutoff (clamped, 0.54119610f);
    filter2.setCutoff (clamped, q);
    untilUpdate = kUpdateInterval;
}

float SynthEngine::oscillator (double hz) noexcept
{
    const auto dt = (float) (hz / sampleRate);
    const auto t = (float) phase;

    float y = 0.0f;
    switch (settings.wave)
    {
        case Wave::saw:
            y = 2.0f * t - 1.0f - polyBlep (t, dt);
            break;
        case Wave::square:
            y = rkfx::polyBlepSquare (t, dt);
            break;
        case Wave::triangle:
            y = 4.0f * std::abs (t - 0.5f) - 1.0f;
            break;
    }

    // Square one octave below.
    const auto subDt = 0.5f * dt;
    const auto subWave = rkfx::polyBlepSquare ((float) subPhase, subDt);

    phase += dt;
    if (phase >= 1.0)
        phase -= 1.0;
    subPhase += subDt;
    if (subPhase >= 1.0)
        subPhase -= 1.0;

    const auto s = sub.getNextValue();
    return (y + s * subWave) / (1.0f + 0.5f * s);
}

void SynthEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    if (numChannels <= 0)
        return;

    const auto toMono = 1.0f / (float) numChannels;
    const auto octaveFactor = std::exp2 ((double) juce::jlimit (-2, 1, settings.octave));

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += io[ch][i];
        mono *= toMono;

        // Threshold, with hysteresis; after silence the next note starts fresh.
        const auto levelDb = rkfx::Dynamics::toDb (gateLevel.process (mono));
        if (gateOpen && levelDb < settings.thresholdDb - kHysteresisDb)
            gateOpen = false;
        else if (! gateOpen && levelDb >= settings.thresholdDb)
            gateOpen = true;

        if (! gateOpen && amp < 1.0e-4f && ! fresh)
        {
            tracker.reset();
            fresh = true;
        }

        // Pitch: a new note jumps straight to its pitch, a legato one glides.
        tracker.process (mono);
        const auto trackedHz = tracker.getHz();
        if (trackedHz > 0.0f)
        {
            const auto target = std::log2 (trackedHz);
            if (fresh)
            {
                logHz = target;
                fresh = false;
                trigger();
            }
            else
            {
                logHz = target + glideCoef * (logHz - target);
            }
        }

        // A fresh attack within a phrase restarts the filter envelope.
        const auto power = mono * mono;
        const auto jumpDb = powerDb (onsetFast.process (power)) - powerDb (onsetSlow.process (power));
        if (armed && jumpDb > kOnsetJumpDb && ! fresh)
        {
            trigger();
            armed = false;
        }
        else if (! armed && jumpDb < 0.5f * kOnsetJumpDb)
        {
            armed = true;
        }

        // Level: steady, or the playing's own.
        const auto level = inputLevel.process (mono);
        const auto sounding = gateOpen && ! fresh;
        const auto ampTarget = sounding ? (1.0f - settings.dynamics) * kSteadyLevel + settings.dynamics * level : 0.0f;
        amp = ampTarget + (ampTarget > amp ? ampAttack : ampRelease) * (amp - ampTarget);

        env *= envDecay;
        envSmooth = env + envAttack * (envSmooth - env);
        if (--untilUpdate <= 0)
            updateFilter();

        auto y = oscillator (std::exp2 ((double) logHz) * octaveFactor);
        y = filter2.process (filter1.process (y, rkfx::StateVariableFilter::Type::lowpass), rkfx::StateVariableFilter::Type::lowpass);
        y = rkfx::softClip (amp * y);

        for (int ch = 0; ch < numChannels; ++ch)
            io[ch][i] = y;
    }
}
