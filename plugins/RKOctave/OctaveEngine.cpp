#include "OctaveEngine.h"

#include "dsp/Dynamics.h"

#include <cmath>

namespace
{
    constexpr float kTrackAttackMs = 0.5f, kTrackReleaseMs = 30.0f;   // filtered level, for the hysteresis
    constexpr float kInputAttackMs = 2.0f, kInputReleaseMs = 40.0f;   // Synth voice level
    constexpr float kGateAttackMs = 1.0f, kGateReleaseMs = 50.0f;     // threshold detector
    constexpr double kFadeInMs = 3.0, kFadeOutMs = 20.0;              // octave voices at the threshold
    constexpr float kHysteresis = 0.3f;        // Schmitt trigger, relative to the filtered level
    constexpr float kTrackRatio = 1.25f;       // tracking low-pass, relative to the fundamental
    constexpr float kHighPassRatio = 0.6f;     // high-pass, relative to the lowest note
    constexpr float kSynthLevel = 0.7f;        // square at 0.7 x the input level ~ the same loudness
    constexpr float kMaxRampMs = 2.0f;
}

OctaveEngine::Limits OctaveEngine::limitsFor (Range r) noexcept
{
    switch (r)
    {
        case Range::bass:   return { 35.0f, 400.0f };     // B0 to the 20th fret of the G string
        case Range::guitar: return { 60.0f, 1100.0f };    // drop C (and cello) to the top frets
        case Range::violin: return { 125.0f, 2700.0f };   // C3 (5-string violin, viola) to E7
    }
    return { 60.0f, 1100.0f };
}

void OctaveEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto* f : { &highPass, &trackLow1, &trackLow2 })
        f->prepare (sampleRate);
    for (auto& f : tone1) f.prepare (sampleRate);
    for (auto& f : tone2) f.prepare (sampleRate);

    trackLevel.prepare (sampleRate);
    trackLevel.setTimes (kTrackAttackMs, kTrackReleaseMs);
    inputLevel.prepare (sampleRate);
    inputLevel.setTimes (kInputAttackMs, kInputReleaseMs);
    gateLevel.prepare (sampleRate);
    gateLevel.setTimes (kGateAttackMs, kGateReleaseMs);

    gateAttack  = (float) std::exp (-1.0 / (kFadeInMs * 0.001 * sampleRate));
    gateRelease = (float) std::exp (-1.0 / (kFadeOutMs * 0.001 * sampleRate));

    for (auto* s : { &direct, &octave1, &octave2, &toneHz })
        s->reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void OctaveEngine::reset()
{
    for (auto* f : { &highPass, &trackLow1, &trackLow2 })
        f->reset();
    for (auto& f : tone1) f.reset();
    for (auto& f : tone2) f.reset();
    trackLevel.reset();
    inputLevel.reset();
    gateLevel.reset();

    direct.setCurrentAndTargetValue (settings.direct);
    octave1.setCurrentAndTargetValue (settings.octave1);
    octave2.setCurrentAndTargetValue (settings.octave2);
    toneHz.setCurrentAndTargetValue (settings.toneHz);

    gateOpen = false;
    gateGain = 0.0f;
    resetTracker();
    updateCoefficients();
}

void OctaveEngine::setSettings (const Settings& s)
{
    const auto rangeChanged = s.range != settings.range;
    settings = s;

    if (rangeChanged)
    {
        limits = limitsFor (s.range);
        resetTracker();
    }

    direct.setTargetValue (s.direct);
    octave1.setTargetValue (s.octave1);
    octave2.setTargetValue (s.octave2);
    toneHz.setTargetValue (s.toneHz);
}

void OctaveEngine::resetTracker() noexcept
{
    high = togglePending = false;
    samplesSinceRise = pendingFor = 0;
    lastBand = 0.0f;
    estimateHz = 0.0f;
    square1 = square2 = edge1 = edge2 = 1.0f;
    step1 = step2 = 1.0f;
    highPass.setCutoff (limits.lowHz * kHighPassRatio);
}

void OctaveEngine::updateCoefficients() noexcept
{
    // Before the first cycle is measured, the low-pass sits just above the
    // lowest note: whatever is played, its fundamental is then the
    // strongest component left.
    const auto fundamental = estimateHz > 0.0f ? estimateHz : limits.lowHz;
    const auto cutoff = juce::jlimit (limits.lowHz, limits.highHz, fundamental) * kTrackRatio;
    const auto g = rkfx::StateVariableFilter::gFor (cutoff, sampleRate);
    trackLow1.setCoefficients (g, 1.0f / 0.70710678f);
    trackLow2.setCoefficients (g, 1.0f / 0.70710678f);

    const auto tone = toneHz.getNextValue();
    toneHz.skip (kUpdateInterval - 1);
    for (auto& f : tone1) f.setCutoff (tone, 0.54119610f);   // two stages: 24 dB/oct Butterworth
    for (auto& f : tone2) f.setCutoff (tone, 1.30656296f);

    untilUpdate = kUpdateInterval;
}

float OctaveEngine::rampSamples() const noexcept
{
    const auto period = (float) sampleRate / (estimateHz > 0.0f ? estimateHz : limits.lowHz);
    return juce::jlimit (2.0f, kMaxRampMs * 0.001f * (float) sampleRate, period / 8.0f);
}

void OctaveEngine::toggle() noexcept
{
    // Octave 1 toggles every cycle, Octave 2 on Octave 1's rising edges.
    const auto step = 2.0f / rampSamples();
    square1 = -square1;
    step1 = step;
    if (square1 > 0.0f)
    {
        square2 = -square2;
        step2 = step;
    }
}

void OctaveEngine::track (float mono) noexcept
{
    const auto band = highPass.process (mono, rkfx::StateVariableFilter::Type::highpass);
    const auto y = trackLow2.process (trackLow1.process (band, rkfx::StateVariableFilter::Type::lowpass),
                                      rkfx::StateVariableFilter::Type::lowpass);
    const auto threshold = kHysteresis * trackLevel.process (y);

    samplesSinceRise = juce::jmin (samplesSinceRise + 1, 1 << 20);

    if (! high && y > threshold && threshold > 1.0e-7f)
    {
        high = true;

        // One rising crossing per cycle: measure the period...
        const auto hz = (float) sampleRate / (float) samplesSinceRise;
        samplesSinceRise = 0;
        if (hz >= limits.lowHz * 0.5f && hz <= limits.highHz * 1.5f)
            estimateHz = estimateHz > 0.0f ? 0.5f * (estimateHz + hz) : hz;

        // ...and divide, at the input's next rising zero crossing.
        togglePending = true;
        pendingFor = 0;
    }
    else if (high && y < -threshold)
    {
        high = false;
    }

    if (togglePending)
    {
        const auto crossed = lastBand <= 0.0f && band > 0.0f;
        const auto period = (float) sampleRate / (estimateHz > 0.0f ? estimateHz : limits.lowHz);
        if (crossed || (float) ++pendingFor > period)   // no crossing found: don't skip a cycle
        {
            togglePending = false;
            toggle();
        }
    }
    lastBand = band;

    edge1 = square1 > edge1 ? juce::jmin (square1, edge1 + step1) : juce::jmax (square1, edge1 - step1);
    edge2 = square2 > edge2 ? juce::jmin (square2, edge2 + step2) : juce::jmax (square2, edge2 - step2);
}

void OctaveEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    if (numChannels <= 0)
        return;

    const auto synth = settings.voice == Voice::synth;
    const auto toMono = 1.0f / (float) numChannels;

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += io[ch][i];
        mono *= toMono;

        // Threshold, with hysteresis. Once the voices have faded out, the
        // tracker starts over for the next note.
        const auto levelDb = rkfx::Dynamics::toDb (gateLevel.process (mono));
        if (gateOpen && levelDb < settings.thresholdDb - kHysteresisDb)
            gateOpen = false;
        else if (! gateOpen && levelDb >= settings.thresholdDb)
            gateOpen = true;

        const auto gateTarget = gateOpen ? 1.0f : 0.0f;
        gateGain = gateTarget + (gateOpen ? gateAttack : gateRelease) * (gateGain - gateTarget);
        if (! gateOpen && gateGain < 1.0e-4f && estimateHz > 0.0f)
            resetTracker();

        track (mono);
        const auto level = inputLevel.process (mono);

        if (--untilUpdate <= 0)
            updateCoefficients();

        const auto d = direct.getNextValue();
        const auto divided = octave1.getNextValue() * edge1 + octave2.getNextValue() * edge2;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto x = io[ch][i];
            const auto voice = synth ? kSynthLevel * level * divided : x * divided;
            const auto toned = tone2[(size_t) ch].process (tone1[(size_t) ch].process (voice, rkfx::StateVariableFilter::Type::lowpass),
                                                          rkfx::StateVariableFilter::Type::lowpass);
            io[ch][i] = d * x + gateGain * toned;
        }
    }
}
