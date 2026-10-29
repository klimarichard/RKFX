#include "RideEngine.h"

#include <cmath>

using rkfx::Dynamics::kSilenceDb;
using rkfx::Dynamics::timeCoefficient;

namespace
{
    constexpr float kWeightingHighPassHz = 80.0f;
    constexpr double kWeightingShelfHz = 2000.0;
    constexpr double kWeightingShelfDb = 4.0;
}

void RideEngine::Detector::prepare (double rate)
{
    for (auto& f : highPass)
    {
        f.prepare (rate);
        f.setCutoff (kWeightingHighPassHz);
    }
    shelf.setCoefficients (rkfx::MatchedDesign::highShelf (kWeightingShelfHz, kWeightingShelfDb, rate));
    reset();
}

void RideEngine::Detector::reset()
{
    for (auto& f : highPass)
        f.reset();
    shelf.reset();
    meanSquare = 0.0f;
}

void RideEngine::Detector::process (const float* const* x, int numChannels, int index, float coefficient) noexcept
{
    // Power averaged over the channels (one level for the stereo pair).
    auto power = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto weighted = shelf.process (ch, highPass[(size_t) ch].process (x[ch][index], rkfx::StateVariableFilter::Type::highpass));
        power += weighted * weighted;
    }
    power /= (float) juce::jmax (1, numChannels);

    meanSquare = power + coefficient * (meanSquare - power);
}

float RideEngine::Detector::levelDb() const noexcept
{
    return meanSquare > 1.0e-12f ? 10.0f * std::log10 (meanSquare) : kSilenceDb;
}

//==============================================================================
void RideEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    windowCoefficient = timeCoefficient (kWindowMs, sampleRate);
    input.prepare (sampleRate);
    music.prepare (sampleRate);
    reset();
}

void RideEngine::reset()
{
    input.reset();
    music.reset();
    gainDb = wantedDb = 0.0f;
    glideCoefficient = timeCoefficient (settings.speedMs, sampleRate);
    silentMs = 0.0f;
    countdown = 0;
    meter = {};
    meter.targetDb = settings.targetDb;
}

void RideEngine::setSettings (const Settings& s)
{
    settings = s;
}

RideEngine::Meter RideEngine::takeMeter() noexcept
{
    const auto m = meter;
    meter.levelDb = kSilenceDb;
    return m;
}

void RideEngine::updateGainComputer (bool hasSidechain) noexcept
{
    const auto& s = settings;
    const auto levelDb = input.levelDb();
    const auto intervalMs = 1000.0f * (float) kUpdateInterval / (float) sampleRate;

    auto targetDb = s.targetDb;
    if (s.relative && hasSidechain)
    {
        const auto musicDb = music.levelDb();
        if (musicDb > kSidechainSilentDb)
            targetDb = musicDb + s.aboveDb;
    }

    meter.levelDb = juce::jmax (meter.levelDb, levelDb);
    meter.targetDb = targetDb;

    if (levelDb < s.floorDb)
    {
        // A pause: hold, then after Return drift back to 0 dB.
        silentMs += intervalMs;
        meter.holding = true;

        if (silentMs < s.returnMs)
        {
            wantedDb = gainDb;
            glideCoefficient = 0.0f;
            return;
        }

        wantedDb = 0.0f;
        glideCoefficient = timeCoefficient (kReturnGlideMs, sampleRate);
        return;
    }

    silentMs = 0.0f;
    meter.holding = false;

    wantedDb = juce::jlimit (-s.maxCutDb, s.maxBoostDb, targetDb - levelDb);

    // Down twice as fast as up, as an engineer rides.
    const auto down = wantedDb < gainDb;
    glideCoefficient = timeCoefficient (down ? s.speedMs * 0.5f : s.speedMs, sampleRate);
}

void RideEngine::process (float* const* io, int numChannels, int numSamples,
                          const float* const* sidechain, int sidechainChannels) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    const auto hasSidechain = sidechain != nullptr && sidechainChannels > 0;
    const auto scChannels = juce::jmin (sidechainChannels, kMaxChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        input.process (io, numChannels, i, windowCoefficient);
        if (settings.relative && hasSidechain)
            music.process (sidechain, scChannels, i, windowCoefficient);

        if (--countdown <= 0)
        {
            countdown = kUpdateInterval;
            updateGainComputer (hasSidechain);
        }

        gainDb = wantedDb + glideCoefficient * (gainDb - wantedDb);
        const auto gain = std::exp (gainDb * 0.11512925f);   // dB to gain: ln(10) / 20

        for (int ch = 0; ch < numChannels; ++ch)
            io[ch][i] *= gain;
    }

    meter.gainDb = gainDb;
}
