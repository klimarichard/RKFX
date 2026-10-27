#include "PhaseEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

void PhaseEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& c : channels)
        c.dc.prepare (sampleRate);
    for (auto& lfo : lfos)
    {
        lfo.prepare (sampleRate);
        lfo.setShape (rkfx::LFO::Shape::sine);
    }
    envelope.prepare (sampleRate);

    centreOctave.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    depth.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    feedback.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    envAmount.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    envSens.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    fadeLength = juce::jmax (1, (int) (0.01 * sampleRate));

    setSettings (settings);
    reset();
}

void PhaseEngine::reset()
{
    for (auto& c : channels)
    {
        c.x1.fill (0.0f);
        c.y1.fill (0.0f);
        c.dc.reset();
        c.lastOut = 0.0f;
    }

    lfos[0].reset();
    lfos[1].reset (settings.phaseDeg / 360.0);
    envelope.reset();

    for (auto* s : { &centreOctave, &depth, &feedback, &envAmount })
        s->setCurrentAndTargetValue (s->getTargetValue());
    envSens.setCurrentAndTargetValue (envSens.getTargetValue());

    previousStages = stages = juce::jlimit (1, kMaxStages, settings.stages);
    fadeRemaining = 0;
}

void PhaseEngine::setSettings (const Settings& s)
{
    settings = s;
    centreOctave.setTargetValue (std::log2 (juce::jlimit (20.0f, 20000.0f, s.centreHz)));
    depth.setTargetValue (juce::jlimit (0.0f, 1.0f, s.depth));
    feedback.setTargetValue (juce::jlimit (-0.95f, 0.95f, s.feedback));
    envAmount.setTargetValue (juce::jlimit (-1.0f, 1.0f, s.envAmount));
    envSens.setTargetValue (juce::Decibels::decibelsToGain (s.envSensDb));
    envelope.setTimes (s.envAttackMs, s.envReleaseMs);
}

void PhaseEngine::process (float* const* io, int numChannels, int numSamples, const rkfx::Transport& transport) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    // A stage count change crossfades from the old output tap to the new one.
    const auto wanted = juce::jlimit (1, kMaxStages, settings.stages);
    if (wanted != stages && fadeRemaining == 0)
    {
        // Stages coming into use start from silence.
        for (auto& c : channels)
            for (int s = stages; s < wanted; ++s)
                c.x1[(size_t) s] = c.y1[(size_t) s] = 0.0f;

        previousStages = stages;
        stages = wanted;
        fadeRemaining = fadeLength;
    }

    double rate = settings.rateHz;
    if (settings.sync)
    {
        const auto beats = rkfx::SyncDivision::beats (settings.division);
        rate = transport.bpm / 60.0 / beats;
        if (transport.hasPosition && transport.playing)
            lfos[0].syncTo (transport.ppq, beats);
    }
    for (auto& lfo : lfos)
        lfo.setRate (rate);
    lfos[1].syncTo (lfos[0].getPhase(), 1.0, settings.phaseDeg / 360.0);

    const auto minOctave = std::log2 (20.0f);
    const auto maxOctave = (float) std::log2 (juce::jmin (20000.0, 0.45 * sampleRate));
    constexpr auto pi = juce::MathConstants<double>::pi;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto centre = centreOctave.getNextValue();
        const auto d = depth.getNextValue();
        const auto fb = feedback.getNextValue();
        const auto env = envAmount.getNextValue();
        const auto sens = envSens.getNextValue();

        // Stereo-linked envelope: the louder channel drives both.
        float detector = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            detector = juce::jmax (detector, std::abs (io[ch][i]));
        const auto level = envelope.process (detector * sens);
        const auto envelope01 = std::abs (env) > 0.0f ? rkfx::EnvelopeFollower::toUnit (level) : 0.0f;

        const auto running = juce::jmax (stages, fadeRemaining > 0 ? previousStages : 0);
        const auto fade = fadeRemaining > 0 ? 1.0f - (float) fadeRemaining / (float) fadeLength : 1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];

            const auto octave = juce::jlimit (minOctave, maxOctave,
                                              centre + kLfoOctaves * d * lfos[(size_t) ch].next() + kEnvOctaves * env * envelope01);
            const auto t = (float) std::tan (pi * std::exp2 ((double) octave) / sampleRate);
            const auto a = (t - 1.0f) / (t + 1.0f);

            auto x = io[ch][i] + fb * rkfx::softClip (c.dc.process (c.lastOut));
            float tapOld = x, tapNew = x;

            for (int s = 0; s < running; ++s)
            {
                const auto y = a * x + c.x1[(size_t) s] - a * c.y1[(size_t) s];
                c.x1[(size_t) s] = x;
                c.y1[(size_t) s] = y;
                x = y;

                if (s + 1 == previousStages) tapOld = y;
                if (s + 1 == stages)         tapNew = y;
            }

            const auto out = fadeRemaining > 0 ? tapOld + fade * (tapNew - tapOld) : tapNew;
            c.lastOut = out;
            io[ch][i] = out;
        }

        if (numChannels == 1)
            lfos[1].next();

        if (fadeRemaining > 0)
            --fadeRemaining;
    }
}
