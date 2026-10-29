#include "DelayEngine.h"

#include "safety/SoftClip.h"

#include <cmath>

namespace
{
    constexpr double kTwoPi = 6.28318530717958647692;

    // Character settings per mode: extra loop low-pass (Hz, 0 = none) and saturation amount.
    constexpr float kAnalogLowPassHz = 3500.0f;
    constexpr float kTapeLowPassHz = 9000.0f;
    constexpr float kAnalogDrive = 1.6f;
    constexpr float kTapeDrive = 1.25f;

    // Keeps an Octave down shimmer from piling up low end.
    constexpr float kShimmerHighPassHz = 80.0f;

    // Tape wow (slow, random + periodic) and flutter (fast, periodic), in ms.
    constexpr float kWowMs = 0.35f, kFlutterMs = 0.04f;
    constexpr double kWowHz = 0.45, kFlutterHz = 6.5;

    // Modulation knob at 100%: +/-2 ms.
    constexpr float kMaxModMs = 2.0f;
}

double DelayEngine::leftTimeMs (const Settings& s, double bpm)
{
    const auto ms = s.sync ? rkfx::SyncDivision::beats (s.division) * 60000.0 / juce::jmax (1.0, bpm)
                           : (double) s.timeMs;
    return juce::jlimit (1.0, kMaxSeconds * 1000.0, ms);
}

void DelayEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    // Room for the longest time with the right channel's +50% offset, plus modulation.
    const auto maxSamples = (int) std::ceil (kMaxSeconds * 1.5 * sampleRate + 0.01 * sampleRate);
    for (auto& c : channels)
    {
        c.line.prepare (maxSamples);
        c.lowCut.prepare (sampleRate);
        c.highCut.prepare (sampleRate);
        c.dc.prepare (sampleRate);
        c.shifter.prepare (sampleRate);
        c.shimmerHighPass.prepare (sampleRate);
        c.shimmerHighPass.setCutoff (kShimmerHighPassHz);
    }

    wow.prepare (sampleRate);
    wow.setRate ((float) kWowHz);
    inputLevel.prepare (sampleRate);
    inputLevel.setTimes (5.0f, 250.0f);

    timeCoef = (float) std::exp (-1.0 / (0.08 * sampleRate));   // ~80 ms glide on time changes

    for (auto* s : { &feedback, &darkCoef, &saturation, &tapeAmount, &modDepthSamples, &duck, &shimmer })
        s->reset (sampleRate, rkfx::kParamSmoothingSeconds);
    lowCutHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    highCutHz.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void DelayEngine::reset()
{
    for (auto& c : channels)
    {
        c.line.reset();
        c.lowCut.reset();
        c.highCut.reset();
        c.dc.reset();
        c.shifter.reset();
        c.shimmerHighPass.reset();
        c.characterLowPass = 0.0f;
        c.lastProcessed = 0.0f;
        c.time = -1.0;   // jump straight to the set time on the first sample
    }

    wow.reset();
    inputLevel.reset();
    wowPhase = flutterPhase = modPhase = 0.0;

    shimmerRunning = false;

    for (auto* s : { &feedback, &darkCoef, &saturation, &tapeAmount, &modDepthSamples, &duck, &shimmer })
        s->setCurrentAndTargetValue (s->getTargetValue());
    lowCutHz.setCurrentAndTargetValue (lowCutHz.getTargetValue());
    highCutHz.setCurrentAndTargetValue (highCutHz.getTargetValue());
}

void DelayEngine::setSettings (const Settings& s)
{
    settings = s;

    feedback.setTargetValue (juce::jlimit (0.0f, 1.0f, s.feedback));
    lowCutHz.setTargetValue (juce::jlimit (10.0f, 20000.0f, s.lowCutHz));
    highCutHz.setTargetValue (juce::jlimit (200.0f, 20000.0f, s.highCutHz));
    modDepthSamples.setTargetValue (juce::jlimit (0.0f, 1.0f, s.modDepth) * kMaxModMs * 0.001f * (float) sampleRate);
    duck.setTargetValue (juce::jlimit (0.0f, 1.0f, s.duck));
    shimmer.setTargetValue (juce::jlimit (0.0f, 1.0f, s.shimmer));
    for (auto& c : channels)
        c.shifter.setRatio (juce::jlimit (0.25f, 4.0f, s.shimmerRatio));

    // Character: one-pole coefficient of the extra loop low-pass (0 = open), saturation, tape amount.
    const auto onePole = [this] (float hz) { return (float) std::exp (-kTwoPi * hz / sampleRate); };
    switch (s.mode)
    {
        case Mode::digital: darkCoef.setTargetValue (0.0f);                      saturation.setTargetValue (0.0f); tapeAmount.setTargetValue (0.0f); break;
        case Mode::analog:  darkCoef.setTargetValue (onePole (kAnalogLowPassHz)); saturation.setTargetValue (1.0f); tapeAmount.setTargetValue (0.0f); break;
        case Mode::tape:    darkCoef.setTargetValue (onePole (kTapeLowPassHz));   saturation.setTargetValue (1.0f); tapeAmount.setTargetValue (1.0f); break;
    }
}

void DelayEngine::process (float* const* io, int numChannels, int numSamples, const rkfx::Transport& transport) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    const auto leftMs = leftTimeMs (settings, transport.bpm);
    const auto rightMs = juce::jlimit (1.0, kMaxSeconds * 1500.0, leftMs * (1.0 + (double) juce::jlimit (-0.5f, 0.5f, settings.offset)));
    const double target[kMaxChannels] = { leftMs * 0.001 * sampleRate, rightMs * 0.001 * sampleRate };
    const auto maxDelay = (double) channels[0].line.getMaxDelay() - 2.0;

    for (auto& c : channels)
    {
        c.lowCut.setCutoff (lowCutHz.getCurrentValue());
        c.highCut.setCutoff (highCutHz.getCurrentValue());
    }
    lowCutHz.skip (numSamples);
    highCutHz.skip (numSamples);

    const auto drive = settings.mode == Mode::analog ? kAnalogDrive : kTapeDrive;
    const auto msToSamples = (float) (0.001 * sampleRate);
    const auto stereoMode = numChannels < 2 ? Stereo::mono : settings.stereo;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto fb = feedback.getNextValue();
        const auto dark = darkCoef.getNextValue();
        const auto sat = saturation.getNextValue();
        const auto tape = tapeAmount.getNextValue();
        const auto mod = modDepthSamples.getNextValue();
        const auto duckAmount = duck.getNextValue();
        const auto shimmerAmount = shimmer.getNextValue();

        const auto shimmering = shimmerAmount > 0.0f || shimmer.isSmoothing();
        if (shimmering && ! shimmerRunning)
            for (auto& c : channels)
            {
                c.shifter.reset();
                c.shimmerHighPass.reset();
            }
        shimmerRunning = shimmering;

        // Time modulation: the modulation knob (sine) plus tape wow and flutter.
        const auto modulation = mod * (float) std::sin (kTwoPi * modPhase)
                              + tape * msToSamples * (kWowMs * (0.6f * wow.next() + 0.4f * (float) std::sin (kTwoPi * wowPhase))
                                                      + kFlutterMs * (float) std::sin (kTwoPi * flutterPhase));
        modPhase += settings.modRateHz / sampleRate;       modPhase -= std::floor (modPhase);
        wowPhase += kWowHz / sampleRate;                    wowPhase -= std::floor (wowPhase);
        flutterPhase += kFlutterHz / sampleRate;            flutterPhase -= std::floor (flutterPhase);

        // Input: mono sum for mono and ping-pong modes.
        float in[kMaxChannels] {};
        float detector = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            in[ch] = io[ch][i];
            detector = juce::jmax (detector, std::abs (in[ch]));
        }
        if (stereoMode != Stereo::stereo && numChannels == 2)
            in[0] = in[1] = 0.5f * (in[0] + in[1]);

        // Read every channel's repeat and run it through the character and cut filters.
        float processed[kMaxChannels] {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const auto wanted = stereoMode == Stereo::mono ? target[0] : target[ch];
            c.time = c.time < 0.0 ? wanted : wanted + timeCoef * (c.time - wanted);

            // Read before writing, so the loop's one-sample delay is accounted for.
            auto y = c.line.read ((float) juce::jlimit (2.0, maxDelay, c.time + modulation) - 1.0f);

            c.characterLowPass = y + dark * (c.characterLowPass - y);
            y = c.characterLowPass;
            if (sat > 0.0f)
                y += sat * (rkfx::softClip (y * drive) / drive - y);

            y = c.lowCut.process (y, rkfx::StateVariableFilter::Type::highpass);
            y = c.highCut.process (y, rkfx::StateVariableFilter::Type::lowpass);
            processed[ch] = y;
        }

        // Feedback, routed according to the stereo mode.
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];

            float input = in[ch];
            int source = ch;
            if (stereoMode == Stereo::pingPong)
            {
                // Left takes the input and the right's repeats; right takes the left's repeats.
                source = 1 - ch;
                input = ch == 0 ? in[0] : 0.0f;
            }
            else if (stereoMode == Stereo::mono)
            {
                source = 0;
            }

            const auto repeat = c.dc.process (fb * processed[source]);

            if (! shimmering)
            {
                c.line.write (input + rkfx::softClip (repeat));
                continue;
            }

            // Shimmer: the shifted part goes in the shifter's latency early, so
            // its trip around the loop still takes the delay time.
            c.line.write (input + rkfx::softClip ((1.0f - shimmerAmount) * repeat));

            auto shifted = c.shifter.process (shimmerAmount * repeat);
            shifted = rkfx::softClip (c.shimmerHighPass.process (shifted, rkfx::StateVariableFilter::Type::highpass));
            const auto early = juce::jlimit (0, juce::jmax (0, (int) c.time - 3), c.shifter.latencySamples());
            c.line.addAt (early, shifted);
        }

        // Ducking: the louder the input, the quieter the repeats.
        const auto inputDb01 = rkfx::EnvelopeFollower::toUnit (inputLevel.process (detector));
        const auto duckGain = 1.0f - duckAmount * inputDb01;

        for (int ch = 0; ch < numChannels; ++ch)
            io[ch][i] = duckGain * processed[stereoMode == Stereo::mono ? 0 : ch];
    }
}
