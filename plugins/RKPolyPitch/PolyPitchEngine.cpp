#include "PolyPitchEngine.h"

#include <cmath>

namespace
{
    // Per range (violin, guitar, bass): the search covers half a period of the lowest
    // note (G3 196 Hz, E2 82 Hz, E1 41 Hz), the window about one and a half periods.
    constexpr float kRangeWindowMs[] = { 8.0f, 16.0f, 30.0f };
    constexpr float kRangeSearchMs[] = { 2.6f, 6.1f, 12.2f };
    constexpr int kUpdateInterval = 16;                           // samples between shift updates
}

float PolyPitchEngine::windowMs (Range r, bool lowLatency)
{
    return kRangeWindowMs[(int) r] * (lowLatency ? kLowLatencyFactor : 1.0f);
}

float PolyPitchEngine::searchMs (Range r)
{
    return kRangeSearchMs[(int) r];
}

float PolyPitchEngine::delayMs (Range r, bool lowLatency)
{
    // The shifter's average delay: the search span plus half the window.
    return searchMs (r) + 0.5f * windowMs (r, lowLatency);
}

void PolyPitchEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    main.prepare (sampleRate, kRangeWindowMs[2], kRangeSearchMs[2]);
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        up[(size_t) ch].prepare (sampleRate, kDetuneWindowMs, kDetuneWindowMs * kSecondWindow);
        down[(size_t) ch].prepare (sampleRate, kDetuneWindowMs, kDetuneWindowMs * kSecondWindow);
        for (auto* t : { &toneMain[(size_t) ch], &toneUp[(size_t) ch], &toneDown[(size_t) ch] })
            t->prepare (sampleRate);
    }
    for (auto* s : { &dryGain, &mainGain, &detuneGain })
        s->reset (sampleRate, rkfx::kParamSmoothingSeconds);

    appliedWindowMs = 0.0f;
    setSettings (settings);
    reset();
}

void PolyPitchEngine::reset()
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        for (auto* s : { &up[(size_t) ch], &down[(size_t) ch] })
            s->reset();
        for (auto* t : { &toneMain[(size_t) ch], &toneUp[(size_t) ch], &toneDown[(size_t) ch] })
            t->reset();
    }
    for (auto* s : { &dryGain, &mainGain, &detuneGain })
        s->setCurrentAndTargetValue (s->getTargetValue());

    pedal = juce::jlimit (0.0f, 1.0f, settings.pedal);
    momentaryPosition = settings.momentary ? 1.0f : 0.0f;
    appliedWindowMs = pendingWindowMs = windowMs (settings.range, settings.lowLatency);
    appliedRange = settings.range;
    main.setWindow (appliedWindowMs, searchMs (appliedRange));
    main.reset();
    windowFade = 1.0f;
}

void PolyPitchEngine::setSettings (const Settings& s)
{
    settings = s;

    const auto voice = juce::Decibels::decibelsToGain (s.voiceGainDb, -60.0f);
    dryGain.setTargetValue (s.mode == Mode::whammy ? 0.0f : 1.0f);
    mainGain.setTargetValue (s.mode == Mode::whammy ? 1.0f : s.mode == Mode::harmony ? voice : 0.0f);
    detuneGain.setTargetValue (s.mode == Mode::detune ? voice : 0.0f);

    for (int ch = 0; ch < kMaxChannels; ++ch)
        for (auto* t : { &toneMain[(size_t) ch], &toneUp[(size_t) ch], &toneDown[(size_t) ch] })
            t->setCutoff (juce::jlimit (200.0f, 20000.0f, s.toneHz));

    const auto detuneRatio = std::exp2 (juce::jlimit (0.0f, 50.0f, s.detuneCents) / 1200.0f);
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        up[(size_t) ch].setRatio (detuneRatio);
        down[(size_t) ch].setRatio (1.0f / detuneRatio);
    }

    pendingWindowMs = windowMs (s.range, s.lowLatency);
}

void PolyPitchEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jlimit (1, kMaxChannels, numChannels);

    const auto glide = settings.glideMs <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (0.001 * settings.glideMs * sampleRate));
    const auto riseStep = 1.0f / juce::jmax (1.0f, settings.riseMs * 0.001f * (float) sampleRate);
    const auto fallStep = 1.0f / juce::jmax (1.0f, settings.fallMs * 0.001f * (float) sampleRate);
    const auto fadeStep = 1.0f / (kWindowFadeMs * 0.001f * (float) sampleRate);
    const auto target = juce::jlimit (0.0f, 1.0f, settings.pedal);
    const auto pan = juce::jlimit (0.0f, 1.0f, settings.width);

    for (int i = 0; i < numSamples; ++i)
    {
        // The window: fade the shifted voice out, change, fade back in.
        if (! juce::exactlyEqual (pendingWindowMs, appliedWindowMs) || settings.range != appliedRange)
        {
            windowFade -= fadeStep;
            if (windowFade <= 0.0f)
            {
                windowFade = 0.0f;
                appliedWindowMs = pendingWindowMs;
                appliedRange = settings.range;
                main.setWindow (appliedWindowMs, searchMs (appliedRange));
                main.reset();
            }
        }
        else if (windowFade < 1.0f)
        {
            windowFade = juce::jmin (1.0f, windowFade + fadeStep);
        }

        // The pedal (glided) and the momentary switch, every kUpdateInterval samples.
        pedal = target + glide * (pedal - target);
        momentaryPosition = settings.momentary ? juce::jmin (1.0f, momentaryPosition + riseStep)
                                               : juce::jmax (0.0f, momentaryPosition - fallStep);
        if (i % kUpdateInterval == 0)
        {
            const auto position = pedal + momentaryPosition * (1.0f - pedal);
            const auto shift = juce::jlimit (-24.0f, 24.0f, settings.heel + (settings.toe - settings.heel) * position
                                                           + juce::jlimit (-50.0f, 50.0f, settings.fineCents) * 0.01f);
            main.setRatio (std::exp2 (shift / 12.0f));
            shownShift = shift;
            shownPosition = position;
        }

        const auto d = dryGain.getNextValue();
        const auto m = mainGain.getNextValue() * windowFade;
        const auto t = detuneGain.getNextValue();

        // The main voice, both channels at once (they share the alignment).
        float frame[kMaxChannels] {}, shifted[kMaxChannels] {};
        const auto mainOn = m > 0.0f || mainGain.isSmoothing() || mainGain.getTargetValue() > 0.0f;
        if (mainOn)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                frame[ch] = io[ch][i];
            main.process (frame, shifted, numChannels);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto x = io[ch][i];
            auto y = d * x;

            if (mainOn)
                y += m * toneMain[(size_t) ch].process (shifted[ch], rkfx::StateVariableFilter::Type::lowpass);

            if (t > 0.0f || detuneGain.isSmoothing())
            {
                // Up on one side, down on the other, as far apart as Width.
                const auto u = toneUp[(size_t) ch].process (up[(size_t) ch].process (x), rkfx::StateVariableFilter::Type::lowpass);
                const auto w = toneDown[(size_t) ch].process (down[(size_t) ch].process (x), rkfx::StateVariableFilter::Type::lowpass);
                const auto towardUp = numChannels == 2 ? (ch == 0 ? 0.5f + 0.5f * pan : 0.5f - 0.5f * pan) : 0.5f;
                y += t * (towardUp * u + (1.0f - towardUp) * w);
            }

            io[ch][i] = y;
        }
    }
}
