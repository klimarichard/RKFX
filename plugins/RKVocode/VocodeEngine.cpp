#include "VocodeEngine.h"

#include <cmath>

namespace
{
    using Type = rkfx::StateVariableFilter::Type;

    constexpr float kCarrierLevelAttackMs = 2.0f, kCarrierLevelReleaseMs = 30.0f;
    // Levelling: divide each carrier band by its level, never by less than half
    // the current sample (caps the ratio at 2 while the level catches up at an
    // onset), plus a -50 dB floor so near-silence fades out instead of being raised.
    constexpr float kCarrierFloor = 3.0e-3f;
    constexpr double kVoiceFadeMs = 3.0;

    // The band sum lands about 9 dB under the modulator; this brings it level.
    constexpr float kMakeupGain = 2.8f;

    float polyBlep (double t, double dt) noexcept
    {
        if (t < dt)       { t /= dt;               return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt;    return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }
}

void VocodeEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& b : bands)
    {
        for (auto& f : b.analysis)  f.prepare (sampleRate);
        for (auto& f : b.synthesis) f.prepare (sampleRate);
        b.modulatorLevel.prepare (sampleRate);
        b.carrierLevel.prepare (sampleRate);
        b.carrierLevel.setTimes (kCarrierLevelAttackMs, kCarrierLevelReleaseMs);
    }
    for (auto& f : unvoicedFilter) f.prepare (sampleRate);
    for (auto& f : noiseFilter)    f.prepare (sampleRate);
    unvoicedLevel.prepare (sampleRate);

    ampStep = (float) (1.0 / (kVoiceFadeMs * 0.001 * sampleRate));

    unvoiced.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    modulatorMix.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    layoutFade.reset (sampleRate, 0.005);

    setSettings (settings);
    activeBands = settings.bands;
    configureBands();
    reset();
}

void VocodeEngine::reset()
{
    for (auto& b : bands)
    {
        for (auto& f : b.analysis)  f.reset();
        for (auto& f : b.synthesis) f.reset();
        b.modulatorLevel.reset();
        b.carrierLevel.reset();
    }
    for (auto& f : unvoicedFilter) f.reset();
    for (auto& f : noiseFilter)    f.reset();
    unvoicedLevel.reset();
    for (auto& v : voices)
        v = {};
    applyFixedVoice();

    unvoiced.setCurrentAndTargetValue (unvoiced.getTargetValue());
    modulatorMix.setCurrentAndTargetValue (modulatorMix.getTargetValue());
    layoutFade.setCurrentAndTargetValue (1.0f);
    layoutPending = false;
}

void VocodeEngine::configureBands()
{
    const auto n = juce::jlimit (2, kMaxBands, activeBands);
    const auto low = juce::jlimit (20.0, 2000.0, (double) settings.lowHz);
    const auto high = juce::jlimit (low * 2.0, 0.45 * sampleRate, (double) settings.highHz);
    const auto shift = std::pow (2.0, juce::jlimit (-24.0f, 24.0f, settings.formantSemitones) / 12.0);

    // Bands one spacing wide: Q from the spacing in octaves.
    const auto spacing = std::log2 (high / low) / (n - 1);
    const auto ratio = std::pow (2.0, spacing);
    const auto q = (float) (std::sqrt (ratio) / (ratio - 1.0));

    for (int i = 0; i < n; ++i)
    {
        const auto hz = low * std::pow (high / low, (double) i / (n - 1));
        for (auto& f : bands[(size_t) i].analysis)
            f.setCutoff ((float) hz, q);
        for (auto& f : bands[(size_t) i].synthesis)
            f.setCutoff ((float) juce::jmin (hz * shift, 0.45 * sampleRate), q);
    }

    for (auto& f : unvoicedFilter) f.setCutoff ((float) high);
    for (auto& f : noiseFilter)    f.setCutoff ((float) high);

    lowShown = settings.lowHz;
    highShown = settings.highHz;
    formantRatioShown = settings.formantSemitones;
}

void VocodeEngine::setSettings (const Settings& s)
{
    const auto bandsChanged = s.bands != (layoutPending ? pendingLayout.bands : activeBands);
    const auto leftFixedMode = settings.pitchMode == PitchMode::fixed && s.pitchMode == PitchMode::midi;
    settings = s;

    if (leftFixedMode)
        allNotesOff();   // the fixed note stops; MIDI notes take over

    for (auto& b : bands)
        b.modulatorLevel.setTimes (juce::jlimit (0.5f, 200.0f, s.attackMs), juce::jlimit (5.0f, 2000.0f, s.releaseMs));
    unvoicedLevel.setTimes (juce::jlimit (0.5f, 200.0f, s.attackMs), juce::jlimit (5.0f, 2000.0f, s.releaseMs));

    unvoiced.setTargetValue (juce::jlimit (0.0f, 1.0f, s.unvoiced));
    modulatorMix.setTargetValue (juce::jlimit (0.0f, 1.0f, s.modulatorMix));
    glideCoef = s.glideMs <= 0.0f ? 0.0 : std::exp (-1.0 / (s.glideMs * 0.001 * sampleRate));

    // A new band count fades out, swaps the bands, and fades back in (no click).
    if (bandsChanged)
    {
        pendingLayout = s;
        layoutPending = true;
        layoutFade.setTargetValue (0.0f);
    }
    else if (! layoutPending && (std::abs (s.lowHz - lowShown) > 1.0e-3f || std::abs (s.highHz - highShown) > 1.0e-3f
                                 || std::abs (s.formantSemitones - formantRatioShown) > 1.0e-4f))
    {
        configureBands();   // frequencies glide with their (smoothed) parameters
    }

    applyFixedVoice();
}

void VocodeEngine::applyFixedVoice() noexcept
{
    if (settings.pitchMode != PitchMode::fixed)
        return;

    // One voice, always on, at the fixed note.
    auto& v = voices[0];
    v.note = settings.fixedNote;
    v.held = true;
    v.targetHz = noteHz (settings.fixedNote);
    if (v.hz <= 0.0)
        v.hz = v.targetHz;
    for (size_t i = 1; i < voices.size(); ++i)
        voices[i].held = false;
}

void VocodeEngine::noteOn (int note, float velocity) noexcept
{
    juce::ignoreUnused (velocity);
    if (settings.pitchMode != PitchMode::midi)
        return;

    const auto count = juce::jlimit (1, kMaxVoices, settings.voices);

    // Reuse a voice already on this note, else a free one, else the oldest.
    Voice* chosen = nullptr;
    for (int i = 0; i < count && chosen == nullptr; ++i)
        if (voices[(size_t) i].note == note)
            chosen = &voices[(size_t) i];
    for (int i = 0; i < count && chosen == nullptr; ++i)
        if (! voices[(size_t) i].held && voices[(size_t) i].amp <= 0.0f)
            chosen = &voices[(size_t) i];
    if (chosen == nullptr)
    {
        chosen = &voices[0];
        for (int i = 1; i < count; ++i)
            if (voices[(size_t) i].age < chosen->age)
                chosen = &voices[(size_t) i];
    }

    chosen->note = note;
    chosen->held = true;
    chosen->age = ++voiceClock;
    chosen->targetHz = noteHz (note);
    if (chosen->hz <= 0.0 || glideCoef <= 0.0)
        chosen->hz = chosen->targetHz;   // glide only from a pitch it already had
}

void VocodeEngine::noteOff (int note) noexcept
{
    if (settings.pitchMode != PitchMode::midi)
        return;
    for (auto& v : voices)
        if (v.note == note)
            v.held = false;
}

void VocodeEngine::allNotesOff() noexcept
{
    for (auto& v : voices)
        v.held = false;
}

float VocodeEngine::nextNoise() noexcept
{
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return (float) (noiseSeed >> 8) / 8388608.0f - 1.0f;
}

float VocodeEngine::oscillators() noexcept
{
    float sum = 0.0f;
    const auto noise = nextNoise();

    for (auto& v : voices)
    {
        // Click-free start and stop.
        v.amp = v.held ? juce::jmin (1.0f, v.amp + ampStep) : juce::jmax (0.0f, v.amp - ampStep);
        if (v.amp <= 0.0f)
            continue;

        v.hz = v.targetHz + glideCoef * (v.hz - v.targetHz);
        const auto dt = v.hz / sampleRate;

        float wave = 0.0f;
        switch (settings.wave)
        {
            case Wave::saw:      wave = (float) (2.0 * v.phase - 1.0) - polyBlep (v.phase, dt); break;
            case Wave::pulse:
            {
                wave = v.phase < 0.5 ? 1.0f : -1.0f;
                wave += polyBlep (v.phase, dt);
                auto shifted = v.phase + 0.5;
                if (shifted >= 1.0) shifted -= 1.0;
                wave -= polyBlep (shifted, dt);
                break;
            }
            case Wave::noise:    wave = noise; break;
            case Wave::sawNoise: wave = 0.7f * ((float) (2.0 * v.phase - 1.0) - polyBlep (v.phase, dt)) + 0.3f * noise; break;
        }

        sum += v.amp * wave;
        v.phase += dt;
        if (v.phase >= 1.0)
            v.phase -= 1.0;
    }

    return sum;
}

void VocodeEngine::process (float* const* io, int numChannels, int numSamples,
                            const float* const* sidechain, int sidechainChannels) noexcept
{
    if (numChannels <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        // Band count change: once faded out, swap and fade back in.
        const auto fade = layoutFade.getNextValue();
        if (layoutPending && fade <= 0.0f)
        {
            activeBands = pendingLayout.bands;
            configureBands();
            for (auto& b : bands)
            {
                for (auto& f : b.analysis)  f.reset();
                for (auto& f : b.synthesis) f.reset();
                b.modulatorLevel.reset();
                b.carrierLevel.reset();
            }
            layoutPending = false;
            layoutFade.setTargetValue (1.0f);
        }

        float modulator = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            modulator += io[ch][i];
        modulator /= (float) numChannels;

        float carrier = 0.0f;
        if (settings.carrier == Carrier::sidechain)
        {
            if (sidechain != nullptr && sidechainChannels > 0)
            {
                for (int ch = 0; ch < sidechainChannels; ++ch)
                    carrier += sidechain[ch][i];
                carrier /= (float) sidechainChannels;
            }
        }
        else
        {
            carrier = oscillators();
        }

        // The vocoder bands.
        float out = 0.0f;
        for (int b = 0; b < activeBands; ++b)
        {
            auto& band = bands[(size_t) b];
            auto m = modulator;
            for (auto& f : band.analysis)
                m = f.processNormalisedBandpass (m);
            auto c = carrier;
            for (auto& f : band.synthesis)
                c = f.processNormalisedBandpass (c);

            const auto modLevel = band.modulatorLevel.process (m);
            const auto carLevel = band.carrierLevel.process (c);
            out += c * modLevel / (juce::jmax (carLevel, 0.5f * std::abs (c)) + kCarrierFloor);
        }

        // Unvoiced: the modulator's highs above the top band, plus noise shaped by them.
        auto highs = modulator;
        for (auto& f : unvoicedFilter)
            highs = f.process (highs, Type::highpass);
        auto noise = nextNoise();
        for (auto& f : noiseFilter)
            noise = f.process (noise, Type::highpass);
        const auto highsLevel = unvoicedLevel.process (highs);

        out = fade * kMakeupGain * out + unvoiced.getNextValue() * (highs + 2.0f * noise * highsLevel)
            + modulatorMix.getNextValue() * modulator;

        for (int ch = 0; ch < numChannels; ++ch)
            io[ch][i] = out;
    }
}
