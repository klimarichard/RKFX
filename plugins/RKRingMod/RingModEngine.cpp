#include "RingModEngine.h"

#include "dsp/PolyBlep.h"

#include <cmath>

namespace
{
    constexpr double kGlideMs = 20.0;
    constexpr float kMinHz = 0.05f;
    constexpr float kTrackingFloor = 0.00316f;   // -50 dBFS
    constexpr float kAgreement = 0.06f;          // two cycles within 6%
}

float RingModEngine::carrier (Wave w, float p, float dt) noexcept
{
    switch (w)
    {
        case Wave::sine:     return std::sin (6.28318530718f * p);
        case Wave::triangle: return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
        case Wave::square:   return rkfx::polyBlepSquare (p, dt);
    }
    return 0.0f;
}

void RingModEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    tracker.prepare (sampleRate);
    inputLevel.prepare (sampleRate);
    inputLevel.setTimes (1.0f, 20.0f);
    lfo.prepare (sampleRate);
    lfo.setShape (rkfx::LFO::Shape::sine);
    lfoDepth.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    for (auto& d : dc)
        d.prepare (sampleRate);
    glideCoef = (float) std::exp (-1.0 / (0.001 * kGlideMs * sampleRate));

    setSettings (settings);
    reset();
}

void RingModEngine::reset()
{
    tracker.setRange (settings.range);
    tracker.reset();
    inputLevel.reset();
    lfo.reset();
    lfoDepth.setCurrentAndTargetValue (settings.lfoDepth);
    for (auto& d : dc)
        d.reset();
    phase = 0.0;
    heldTrackedHz = lastCycleHz = 0.0f;
    logHz = std::log2 (juce::jmax (kMinHz, settings.frequencyHz));
}

void RingModEngine::setSettings (const Settings& s)
{
    settings = s;
    tracker.setRange (s.range);
    lfo.setRate (s.lfoRateHz);
    lfoDepth.setTargetValue (s.lfoDepth);
}

void RingModEngine::process (float* const* io, int numChannels, int numSamples, const rkfx::Transport& transport) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    if (numChannels <= 0)
        return;

    const auto toMono = 1.0f / (float) numChannels;
    const auto intervalRatio = std::exp2 (settings.intervalSemitones / 12.0f);

    // Synced: the carrier's rate from the tempo, its phase locked to the beat.
    const auto beats = rkfx::SyncDivision::beats (settings.division);
    const auto syncedHz = (float) (transport.bpm / 60.0 / beats);
    if (settings.mode == Mode::synced && transport.playing && transport.hasPosition)
        phase = transport.ppq / beats - std::floor (transport.ppq / beats);

    for (int i = 0; i < numSamples; ++i)
    {
        float targetHz = settings.frequencyHz;
        if (settings.mode == Mode::tracked)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += io[ch][i];
            mono *= toMono;
            const auto loud = inputLevel.process (mono) > kTrackingFloor;

            // A pitch counts when two cycles agree on a note loud enough to
            // be one (a fading tail or noise mustn't move the carrier).
            if (tracker.process (mono) && tracker.getHz() > 0.0f)
            {
                const auto hz = tracker.getHz();
                if (loud && lastCycleHz > 0.0f && std::abs (hz - lastCycleHz) < kAgreement * hz)
                    heldTrackedHz = hz;
                lastCycleHz = hz;
            }
            if (! loud && lastCycleHz > 0.0f)
            {
                tracker.reset();   // the next note starts afresh
                lastCycleHz = 0.0f;
            }
            targetHz = heldTrackedHz > 0.0f ? heldTrackedHz * intervalRatio : settings.frequencyHz;
        }
        else if (settings.mode == Mode::synced)
        {
            targetHz = syncedHz;
        }

        // Synced follows the tempo exactly; the others glide.
        const auto target = std::log2 (juce::jmax (kMinHz, targetHz));
        logHz = settings.mode == Mode::synced ? target : target + glideCoef * (logHz - target);

        const auto hz = std::exp2 (logHz + lfoDepth.getNextValue() * lfo.next());
        modulatedHz = hz;
        const auto dt = (float) juce::jmin (0.45, hz / sampleRate);
        const auto c = carrier (settings.wave, (float) phase, dt);
        phase += dt;
        if (phase >= 1.0)
            phase -= 1.0;

        for (int ch = 0; ch < numChannels; ++ch)
            io[ch][i] = dc[(size_t) ch].process (io[ch][i] * c);
    }
}
