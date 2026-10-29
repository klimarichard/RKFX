#include "RemapEngine.h"

#include <cmath>

int RemapEngine::leaderOf (const Settings& s, int output)
{
    return (output % 2 == 1 && s.outputLinks[(size_t) (output / 2)]) ? output - 1 : output;
}

RemapEngine::Route RemapEngine::routeOf (const Settings& s, int output)
{
    const auto leader = leaderOf (s, output);
    const auto& o = s.outputs[(size_t) leader];
    const auto linkedOutput = s.outputLinks[(size_t) (output / 2)];

    if (o.source <= kSourceOff || o.source > kFirstPairSource + kPairs - 1)
        return {};

    if (o.source <= kChannels)
        return { o.source - 1, -1 };   // one input (to both sides of a linked pair)

    const auto pair = o.source - kFirstPairSource;
    const auto left = pair * 2, right = left + 1;
    if (! s.inputLinks[(size_t) pair])
        return { left, -1 };           // a pair that isn't linked: its left input alone

    if (linkedOutput && ! o.mono)
        return { output == leader ? left : right, -1 };   // left to left, right to right

    return { left, right };            // a single output, or Mono: the average
}

float RemapEngine::gainFor (int output) const noexcept
{
    const auto& o = settings.outputs[(size_t) leaderOf (settings, output)];
    if (o.mute)
        return 0.0f;
    // 0 dB comes back from the host's 0..1 parameter scale a few millionths off:
    // within 0.001 dB it is unity, so a route stays a bit-exact copy.
    const auto trim = std::abs (o.trimDb) < 0.001f ? 0.0f : juce::jlimit (-24.0f, 12.0f, o.trimDb);
    const auto g = trim == 0.0f ? 1.0f : juce::Decibels::decibelsToGain (trim);
    return o.polarity ? -g : g;
}

void RemapEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    fadeSamples = juce::jmax (1, (int) std::lround (kFadeSeconds * sampleRate));
    inputs.setSize (kChannels, juce::jmax (1, maxBlockSize), false, true, true);
    reset();
}

void RemapEngine::reset()
{
    for (int k = 0; k < kChannels; ++k)
    {
        auto& st = states[(size_t) k];
        st.route = st.previous = routeOf (settings, k);
        st.fadeLeft = 0;
        st.gain = st.targetGain = gainFor (k);
        st.gainStep = 0.0f;
    }
    inputPeaks.fill (0.0f);
    outputPeaks.fill (0.0f);
}

void RemapEngine::setSettings (const Settings& s)
{
    settings = s;

    for (int k = 0; k < kChannels; ++k)
    {
        auto& st = states[(size_t) k];

        const auto route = routeOf (s, k);
        if (route != st.route)
        {
            // A crossfade from what is playing now (mid-fade: from the newer route).
            st.previous = st.route;
            st.route = route;
            st.fadeLeft = fadeSamples;
        }

        const auto target = gainFor (k);
        if (! juce::exactlyEqual (target, st.targetGain))
        {
            st.targetGain = target;
            st.gainStep = (target - st.gain) / (float) fadeSamples;
        }
    }
}

float RemapEngine::takeInputPeak (int channel) noexcept
{
    const auto p = inputPeaks[(size_t) channel];
    inputPeaks[(size_t) channel] = 0.0f;
    return p;
}

float RemapEngine::takeOutputPeak (int channel) noexcept
{
    const auto p = outputPeaks[(size_t) channel];
    outputPeaks[(size_t) channel] = 0.0f;
    return p;
}

void RemapEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kChannels);
    numSamples = juce::jmin (numSamples, inputs.getNumSamples());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        inputs.copyFrom (ch, 0, io[ch], numSamples);
        inputPeaks[(size_t) ch] = juce::jmax (inputPeaks[(size_t) ch], inputs.getMagnitude (ch, 0, numSamples));
    }

    auto readRoute = [this, numChannels] (const Route& r, int i)
    {
        if (r.a < 0 || r.a >= numChannels)
            return 0.0f;
        const auto x = inputs.getSample (r.a, i);
        return r.b < 0 || r.b >= numChannels ? x : 0.5f * (x + inputs.getSample (r.b, i));
    };

    for (int k = 0; k < numChannels; ++k)
    {
        auto& st = states[(size_t) k];
        auto* out = io[k];

        const auto steady = st.fadeLeft == 0 && juce::exactlyEqual (st.gain, st.targetGain);
        if (steady)
        {
            // A plain route: copied as it is (bit-exact at unity), or scaled.
            if (st.route.a < 0 || st.route.a >= numChannels || juce::exactlyEqual (st.gain, 0.0f))
                juce::FloatVectorOperations::clear (out, numSamples);
            else if (st.route.b < 0 || st.route.b >= numChannels)
                juce::FloatVectorOperations::copyWithMultiply (out, inputs.getReadPointer (st.route.a), st.gain, numSamples);
            else
            {
                juce::FloatVectorOperations::copyWithMultiply (out, inputs.getReadPointer (st.route.a), 0.5f * st.gain, numSamples);
                juce::FloatVectorOperations::addWithMultiply (out, inputs.getReadPointer (st.route.b), 0.5f * st.gain, numSamples);
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                auto x = readRoute (st.route, i);
                if (st.fadeLeft > 0)
                {
                    const auto t = 1.0f - (float) st.fadeLeft / (float) fadeSamples;
                    x = t * x + (1.0f - t) * readRoute (st.previous, i);
                    if (--st.fadeLeft == 0)
                        st.previous = st.route;
                }

                if (! juce::exactlyEqual (st.gain, st.targetGain))
                {
                    st.gain += st.gainStep;
                    if ((st.gainStep > 0.0f && st.gain >= st.targetGain) || (st.gainStep < 0.0f && st.gain <= st.targetGain)
                        || juce::exactlyEqual (st.gainStep, 0.0f))
                        st.gain = st.targetGain;
                }
                out[i] = st.gain * x;
            }
        }

        outputPeaks[(size_t) k] = juce::jmax (outputPeaks[(size_t) k], juce::FloatVectorOperations::findMaximum (out, numSamples),
                                              -juce::FloatVectorOperations::findMinimum (out, numSamples));
    }
}
