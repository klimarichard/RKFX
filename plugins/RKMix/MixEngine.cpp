#include "MixEngine.h"

namespace
{
    using FVO = juce::FloatVectorOperations;

    /** A delay of @p d samples (0 = the sample just written), linear between whole samples. */
    inline float readLinear (const rkfx::DelayLine& line, float d) noexcept
    {
        const auto whole = (int) d;
        const auto t = d - (float) whole;
        const auto a = line.readWhole (whole);
        return t <= 0.0f ? a : a + t * (line.readWhole (whole + 1) - a);
    }

    inline void raisePeak (float& peak, const float* data, int numSamples) noexcept
    {
        const auto range = FVO::findMinAndMax (data, numSamples);
        peak = juce::jmax (peak, std::abs (range.getStart()), std::abs (range.getEnd()));
    }
}

void MixEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    const auto maxDelay = (int) std::ceil (kMaxDelayMs * 0.001 * sampleRate) + 2;
    for (auto& s : states)
    {
        for (auto& smoothed : s.input)
            smoothed.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        s.delayFade.amount.reset (sampleRate, kSectionFadeSeconds);
        s.delaySamples.reset (sampleRate, kDelayGlideSeconds);
        for (auto& line : s.delay)
            line.prepare (maxDelay);
        s.fader.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        s.mute.reset (sampleRate, kMuteFadeSeconds);
        s.pan.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        s.toMain.reset (sampleRate, kMuteFadeSeconds);
    }
    mainFader.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    mainBalance.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    monoFader.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    mainMute.reset (sampleRate, kMuteFadeSeconds);
    monoMute.reset (sampleRate, kMuteFadeSeconds);

    channelBuffer.setSize (2, maxBlock);
    dryBuffer.setSize (2, maxBlock);
    postBuffer.setSize (2, maxBlock);
    gainBuffer.setSize (2, maxBlock);
    mainBuffer.setSize (2, maxBlock);
    monoBuffer.setSize (1, maxBlock);

    reset();
}

void MixEngine::reset()
{
    for (int ch = 0; ch < kChannels; ++ch)
        resetChannel (ch);
    applyMain (true);
    channelPeaks.fill (0.0f);
    mainPeaks.fill (0.0f);
    monoPeak = 0.0f;
}

void MixEngine::resetChannel (int ch)
{
    auto& s = states[(size_t) ch];
    for (auto& line : s.delay)
        line.reset();
    applyChannel (ch, true);
}

void MixEngine::setSettings (const Settings& newSettings, std::uint64_t changed)
{
    // A pair linked or unlinked: both channels start clean (a structural change).
    std::uint64_t relinked = 0;
    for (int pair = 0; pair < kPairs; ++pair)
        if (settings.links[(size_t) pair] != newSettings.links[(size_t) pair])
            relinked |= std::uint64_t (3) << (pair * 2);

    settings = newSettings;

    for (int ch = 0; ch < kChannels; ++ch)
    {
        const auto bit = std::uint64_t (1) << ch;
        if ((relinked & bit) != 0)
            resetChannel (ch);
        else if ((changed & bit) != 0)
            applyChannel (ch, false);
    }
    if ((changed & kMainBit) != 0)
        applyMain (false);
}

void MixEngine::setLevels (const Levels& newLevels)
{
    levels = newLevels;
    for (int ch = 0; ch < kChannels; ++ch)
    {
        auto& s = states[(size_t) ch];
        s.fader.setTargetValue (mix::faderGain (levels.channelDb[(size_t) ch]));
        s.mute.setTargetValue (levels.channelMute[(size_t) ch] ? 0.0f : 1.0f);
    }
    mainFader.setTargetValue (mix::faderGain (levels.mainDb));
    mainMute.setTargetValue (levels.mainMute ? 0.0f : 1.0f);
    monoFader.setTargetValue (mix::faderGain (levels.monoDb));
    monoMute.setTargetValue (levels.monoMute ? 0.0f : 1.0f);
}

void MixEngine::applyChannel (int ch, bool snap)
{
    auto& s = states[(size_t) ch];
    const auto& c = settings.channels[(size_t) ch];

    const auto set = [snap] (rkfx::Smoothed& v, float target)
    {
        if (snap)
            v.setCurrentAndTargetValue (target);
        else
            v.setTargetValue (target);
    };

    const auto trim = mix::trimGain (c.trimDb);
    for (int side = 0; side < 2; ++side)
        set (s.input[(size_t) side], c.polarity[(size_t) side] ? -trim : trim);

    if (s.delayFade.set (c.delayOn))
        for (auto& line : s.delay)
            line.reset();
    if (snap)
        s.delayFade.amount.setCurrentAndTargetValue (c.delayOn ? 1.0f : 0.0f);
    set (s.delaySamples, (float) (juce::jlimit (0.0f, kMaxDelayMs, c.delayMs) * 0.001 * sampleRate));

    set (s.pan, c.pan);
    set (s.toMain, c.toMain ? 1.0f : 0.0f);
    if (snap)
    {
        s.fader.setCurrentAndTargetValue (mix::faderGain (levels.channelDb[(size_t) ch]));
        s.mute.setCurrentAndTargetValue (levels.channelMute[(size_t) ch] ? 0.0f : 1.0f);
    }
}

void MixEngine::applyMain (bool snap)
{
    if (snap)
    {
        mainBalance.setCurrentAndTargetValue (settings.main.balance);
        mainFader.setCurrentAndTargetValue (mix::faderGain (levels.mainDb));
        mainMute.setCurrentAndTargetValue (levels.mainMute ? 0.0f : 1.0f);
        monoFader.setCurrentAndTargetValue (mix::faderGain (levels.monoDb));
        monoMute.setCurrentAndTargetValue (levels.monoMute ? 0.0f : 1.0f);
    }
    else
    {
        mainBalance.setTargetValue (settings.main.balance);
    }
}

//==============================================================================
template <typename Process>
void MixEngine::runSection (SectionFade& fade, float* const* io, int numSides, int numSamples, Process&& process) noexcept
{
    if (fade.isOff())
        return;

    if (fade.isFull())
    {
        process (io);
        return;
    }

    for (int side = 0; side < numSides; ++side)
        FVO::copy (dryBuffer.getWritePointer (side), io[side], numSamples);

    process (io);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto amount = fade.amount.getNextValue();
        for (int side = 0; side < numSides; ++side)
        {
            const auto dry = dryBuffer.getSample (side, i);
            io[side][i] = dry + amount * (io[side][i] - dry);
        }
    }
}

void MixEngine::processDelay (ChannelState& s, float* const* io, int numSides, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const auto d = s.delaySamples.getNextValue();
        for (int side = 0; side < numSides; ++side)
        {
            auto& line = s.delay[(size_t) side];
            line.write (io[side][i]);
            io[side][i] = readLinear (line, d);
        }
    }
}

void MixEngine::processChannel (int ch, const float* const* in, int numSamples, bool measure) noexcept
{
    auto& s = states[(size_t) ch];
    const auto numSides = isStereo (ch) ? 2 : 1;
    float* io[2] = { channelBuffer.getWritePointer (0), channelBuffer.getWritePointer (1) };

    // Trim and polarity
    for (int side = 0; side < numSides; ++side)
    {
        auto& gain = s.input[(size_t) side];
        const auto* source = in[ch + side];
        if (gain.isSmoothing())
        {
            for (int i = 0; i < numSamples; ++i)
                io[side][i] = source[i] * gain.getNextValue();
        }
        else if (const auto g = gain.getTargetValue(); juce::exactlyEqual (g, 1.0f))
        {
            FVO::copy (io[side], source, numSamples);
        }
        else
        {
            FVO::copyWithMultiply (io[side], source, g, numSamples);
        }
    }

    // Delay
    runSection (s.delayFade, io, numSides, numSamples, [&] (float* const* buffer) { processDelay (s, buffer, numSides, numSamples); });

    if (measure)
        for (int side = 0; side < numSides; ++side)
            raisePeak (channelPeaks[(size_t) (ch + side)], io[side], numSamples);

    // Mute, fader, pan -> the post-fader stereo signal
    float* post[2] = { postBuffer.getWritePointer (0), postBuffer.getWritePointer (1) };
    const auto gliding = s.fader.isSmoothing() || s.mute.isSmoothing() || s.pan.isSmoothing();

    if (! gliding)
    {
        const auto level = s.fader.getTargetValue() * s.mute.getTargetValue();
        if (level <= 0.0f)
            return;   // silent after the fader: nothing to add

        float left, right;
        if (numSides == 2)
            mix::balanceGains (s.pan.getTargetValue(), left, right);
        else
            mix::monoPanGains (s.pan.getTargetValue(), left, right);

        const float* source[2] = { io[0], io[numSides - 1] };
        const float gains[2] = { left * level, right * level };
        for (int side = 0; side < 2; ++side)
        {
            if (juce::exactlyEqual (gains[side], 1.0f))
                FVO::copy (post[side], source[side], numSamples);
            else
                FVO::copyWithMultiply (post[side], source[side], gains[side], numSamples);
        }
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto level = s.fader.getNextValue() * s.mute.getNextValue();
            float left, right;
            if (numSides == 2)
                mix::balanceGains (s.pan.getNextValue(), left, right);
            else
                mix::monoPanGains (s.pan.getNextValue(), left, right);
            post[0][i] = io[0][i] * left * level;
            post[1][i] = io[numSides - 1][i] * right * level;
        }
    }

    addSwitched (s.toMain, mainBuffer, post, numSamples);
}

void MixEngine::addSwitched (rkfx::Smoothed& gate, juce::AudioBuffer<float>& bus, const float* const* source, int numSamples) noexcept
{
    if (! gate.isSmoothing())
    {
        if (gate.getTargetValue() > 0.0f)
            for (int side = 0; side < 2; ++side)
                FVO::add (bus.getWritePointer (side), source[side], numSamples);
        return;
    }

    auto* left = bus.getWritePointer (0);
    auto* right = bus.getWritePointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = gate.getNextValue();
        left[i] += g * source[0][i];
        right[i] += g * source[1][i];
    }
}

void MixEngine::processMain (float* const* out, int numSamples, bool measure) noexcept
{
    float* main[2] = { mainBuffer.getWritePointer (0), mainBuffer.getWritePointer (1) };
    auto* mono = monoBuffer.getWritePointer (0);

    if (! (mainFader.isSmoothing() || mainMute.isSmoothing() || mainBalance.isSmoothing()))
    {
        float left, right;
        mix::balanceGains (mainBalance.getTargetValue(), left, right);
        const auto level = mainFader.getTargetValue() * mainMute.getTargetValue();
        if (! juce::exactlyEqual (left * level, 1.0f))
            FVO::multiply (main[0], left * level, numSamples);
        if (! juce::exactlyEqual (right * level, 1.0f))
            FVO::multiply (main[1], right * level, numSamples);
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float left, right;
            mix::balanceGains (mainBalance.getNextValue(), left, right);
            const auto level = mainFader.getNextValue() * mainMute.getNextValue();
            main[0][i] *= left * level;
            main[1][i] *= right * level;
        }
    }

    // The mono sum: (L + R) / 2 of the main after its fader.
    if (! (monoFader.isSmoothing() || monoMute.isSmoothing()))
    {
        const auto level = 0.5f * monoFader.getTargetValue() * monoMute.getTargetValue();
        FVO::add (mono, main[0], main[1], numSamples);
        FVO::multiply (mono, level, numSamples);
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
            mono[i] = 0.5f * (main[0][i] + main[1][i]) * monoFader.getNextValue() * monoMute.getNextValue();
    }

    if (measure)
    {
        raisePeak (mainPeaks[0], main[0], numSamples);
        raisePeak (mainPeaks[1], main[1], numSamples);
        raisePeak (monoPeak, mono, numSamples);
    }

    addToTarget (out, settings.main.output, main[0], main[1], numSamples);
    addToTarget (out, settings.main.monoOutput, mono, nullptr, numSamples);
}

void MixEngine::addToTarget (float* const* out, int target, const float* left, const float* right, int numSamples) noexcept
{
    const auto first = mix::firstOutputOf (target);
    if (first < 0)
        return;

    if (mix::isPairTarget (target))
    {
        FVO::add (out[first], left, numSamples);
        FVO::add (out[first + 1], right != nullptr ? right : left, numSamples);
    }
    else if (right == nullptr)
    {
        FVO::add (out[first], left, numSamples);
    }
    else
    {
        // A stereo source on one output: (L + R) / 2.
        FVO::addWithMultiply (out[first], left, 0.5f, numSamples);
        FVO::addWithMultiply (out[first], right, 0.5f, numSamples);
    }
}

//==============================================================================
void MixEngine::process (const float* const* in, float* const* out, int numSamples, bool measure) noexcept
{
    jassert (numSamples <= maxBlock);
    numSamples = juce::jmin (numSamples, maxBlock);

    for (int ch = 0; ch < kChannels; ++ch)
        FVO::clear (out[ch], numSamples);
    mainBuffer.clear (0, numSamples);

    for (int ch = 0; ch < kChannels; ++ch)
        if (! isFollower (ch))
            processChannel (ch, in, numSamples, measure);

    processMain (out, numSamples, measure);
}
