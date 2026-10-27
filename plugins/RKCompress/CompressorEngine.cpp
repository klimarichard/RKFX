#include "CompressorEngine.h"

#include <cmath>

using rkfx::Dynamics::compressorCurve;
using rkfx::Dynamics::toDb;
using rkfx::Dynamics::toGain;

float CompressorEngine::autoMakeupDb (const Settings& s)
{
    return -0.5f * (compressorCurve (0.0f, s.thresholdDb, s.ratio, s.kneeDb) - 0.0f);
}

float CompressorEngine::effectiveMakeupDb (const Settings& s)
{
    return s.autoMakeup ? autoMakeupDb (s) : s.makeupDb;
}

float CompressorEngine::transferDb (const Settings& s, float inputDb)
{
    return compressorCurve (inputDb, s.thresholdDb, s.ratio, s.kneeDb) + effectiveMakeupDb (s);
}

void CompressorEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& d : detectors) d.prepare (sampleRate);
    for (auto& s : smoothers) s.prepare (sampleRate);
    for (auto& f : hpf)       f.prepare (sampleRate);

    for (auto* s : { &threshold, &ratio, &knee, &link, &makeup })
        s->reset (sampleRate, rkfx::kParamSmoothingSeconds);
    listenMix.reset (sampleRate, rkfx::kBypassFadeSeconds);

    setSettings (settings);
    reset();
}

void CompressorEngine::reset()
{
    for (auto& d : detectors) d.reset();
    for (auto& s : smoothers) s.reset();
    for (auto& f : hpf)       f.reset();

    for (auto* s : { &threshold, &ratio, &knee, &link, &makeup, &listenMix })
        s->setCurrentAndTargetValue (s->getTargetValue());

    peakReduction = 0.0f;
    peakInput = rkfx::Dynamics::kSilenceDb;
}

void CompressorEngine::setSettings (const Settings& s)
{
    settings = s;

    threshold.setTargetValue (s.thresholdDb);
    ratio.setTargetValue (juce::jlimit (1.0f, 100.0f, s.ratio));
    knee.setTargetValue (juce::jmax (0.0f, s.kneeDb));
    link.setTargetValue (juce::jlimit (0.0f, 1.0f, s.link));
    makeup.setTargetValue (effectiveMakeupDb (s));
    listenMix.setTargetValue (s.listen ? 1.0f : 0.0f);

    for (auto& d : detectors)
        d.setMode (s.detection);

    for (auto& sm : smoothers)
    {
        sm.setTimes (s.attackMs, s.releaseMs);
        sm.setAutoRelease (s.autoRelease);
    }
}

float CompressorEngine::takeGainReductionDb() noexcept
{
    const auto r = peakReduction;
    peakReduction = 0.0f;
    return r;
}

float CompressorEngine::takeInputLevelDb() noexcept
{
    const auto r = peakInput;
    peakInput = rkfx::Dynamics::kSilenceDb;
    return r;
}

void CompressorEngine::process (float* const* io, int numChannels, int numSamples,
                                const float* const* sidechain, int sidechainChannels) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);

    // Sidechain high-pass: detector only, so switching it never clicks the audio.
    const auto hpfOn = settings.sidechainHpfHz > 0.0f;
    if (hpfOn)
    {
        if (! hpfRunning)
            for (auto& f : hpf)
                f.reset();
        for (auto& f : hpf)
            f.setCutoff (settings.sidechainHpfHz);
    }
    hpfRunning = hpfOn;

    const auto external = settings.external;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = threshold.getNextValue();
        const auto r = ratio.getNextValue();
        const auto k = knee.getNextValue();
        const auto l = link.getNextValue();
        const auto m = makeup.getNextValue();
        const auto listen = listenMix.getNextValue();

        float detector[kMaxChannels] {}, levelDb[kMaxChannels] {};
        float loudest = rkfx::Dynamics::kSilenceDb;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float d = 0.0f;
            if (! external)
                d = io[ch][i];
            else if (sidechain != nullptr && sidechainChannels > 0)
                d = sidechain[juce::jmin (ch, sidechainChannels - 1)][i];

            if (hpfOn)
                d = hpf[(size_t) ch].process (d, rkfx::StateVariableFilter::Type::highpass);

            detector[ch] = d;
            levelDb[ch] = toDb (detectors[(size_t) ch].process (d));
            loudest = juce::jmax (loudest, levelDb[ch]);
        }

        peakInput = juce::jmax (peakInput, loudest);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto inDb = l * loudest + (1.0f - l) * levelDb[ch];
            const auto target = compressorCurve (inDb, t, r, k) - inDb;
            const auto gainDb = smoothers[(size_t) ch].process (target);
            peakReduction = juce::jmin (peakReduction, gainDb);

            auto y = io[ch][i] * toGain (gainDb + m);
            if (listen > 0.0f)
                y += listen * (detector[ch] - y);
            io[ch][i] = y;
        }
    }
}
