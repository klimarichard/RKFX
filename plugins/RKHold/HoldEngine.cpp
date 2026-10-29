#include "HoldEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kTextureMakeup = 1.6f;   // four random-phase grains add up to about -4 dB: made up
    constexpr double kTwoPi = 6.28318530717958647692;

    /** The Hann window, tabulated (a cosine per grain and sample costs more than all the rest). */
    struct HannTable
    {
        static constexpr int kSize = 2048;
        float values[kSize + 1];
        HannTable()
        {
            for (int i = 0; i <= kSize; ++i)
                values[i] = 0.5f - 0.5f * (float) std::cos (kTwoPi * i / kSize);
        }
        float at (float u) const noexcept   // u in 0..1
        {
            const auto x = u * (float) kSize;
            const auto i = juce::jlimit (0, kSize - 1, (int) x);
            const auto t = x - (float) i;
            return values[i] + t * (values[i + 1] - values[i]);
        }
    };
    const HannTable hann;
}

float HoldEngine::nextRandom() noexcept
{
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    return (float) (random & 0xffffff) / (float) 0x1000000;
}

void HoldEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    int size = 16;
    while (size < (int) (kHistorySeconds * sampleRate) + 16)
        size *= 2;
    historyMask = size - 1;
    for (auto& h : history)
        h.assign ((size_t) size, 0.0f);

    const auto captureLength = (int) std::lround (kCaptureSeconds * sampleRate);
    for (auto& layer : layers)
    {
        for (auto& c : layer.capture)
            c.assign ((size_t) captureLength, 0.0f);
        layer.marks.assign (2048, 0);
        layer.length = captureLength;
    }

    tracker.prepare (sampleRate);
    for (auto& t : tone)
        t.prepare (sampleRate);
    holdGain.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    dryGain.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    setSettings (settings);
    reset();
}

void HoldEngine::reset()
{
    for (auto& h : history)
        std::fill (h.begin(), h.end(), 0.0f);
    now = 0;
    markCount = markHead = 0;
    tracker.reset();
    for (auto& layer : layers)
        layer.used = false;
    addedCount = 0;
    for (auto& t : tone)
        t.reset();
    holdGain.setCurrentAndTargetValue (holdGain.getTargetValue());
    dryGain.setCurrentAndTargetValue (dryGain.getTargetValue());
}

void HoldEngine::setSettings (const Settings& s)
{
    settings = s;
    tracker.setRange (s.range);
    for (auto& t : tone)
        t.setCutoff (juce::jlimit (200.0f, 20000.0f, s.toneHz));
    holdGain.setTargetValue (juce::Decibels::decibelsToGain (s.holdGainDb, -60.0f));
    dryGain.setTargetValue (juce::Decibels::decibelsToGain (s.dryGainDb, -60.0f));
    attackStep = 1.0f / juce::jmax (1.0f, s.attackMs * 0.001f * (float) sampleRate);
    releaseStep = 1.0f / juce::jmax (1.0f, s.releaseMs * 0.001f * (float) sampleRate);
}

bool HoldEngine::isHolding() const noexcept
{
    for (const auto& layer : layers)
        if (layer.used && layer.target > 0.0f)
            return true;
    return false;
}

void HoldEngine::releaseAll() noexcept
{
    for (auto& layer : layers)
        layer.target = 0.0f;
}

void HoldEngine::press() noexcept
{
    if (settings.latch && ! settings.add && isHolding())
        releaseAll();
    else
        hold();
}

void HoldEngine::lift() noexcept
{
    if (! settings.latch)
        releaseAll();
}

HoldEngine::LayerInfo HoldEngine::getLayer (int index) const noexcept
{
    // The sounding layers, oldest first.
    std::array<const Layer*, kLayers + 1> sounding {};
    int count = 0;
    for (const auto& layer : layers)
        if (layer.used)
            sounding[(size_t) count++] = &layer;
    std::sort (sounding.begin(), sounding.begin() + count, [] (const Layer* a, const Layer* b) { return a->order < b->order; });

    if (index < 0 || index >= count)
        return {};
    const auto& l = *sounding[(size_t) index];
    return { true, l.note ? l.midiNote : -1, l.gain };
}

bool HoldEngine::hold() noexcept
{
    const auto length = layers[0].length;
    const auto start = now - length;
    if (start < 0)
        return false;

    // Nothing to hold in silence.
    double energy = 0.0;
    for (int i = 0; i < length; ++i)
    {
        auto x = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            x += history[(size_t) ch][(size_t) ((start + i) & historyMask)];
        x /= (float) numChannels;
        energy += (double) x * x;
    }
    const auto rmsDb = 10.0 * std::log10 (energy / length + 1.0e-20);
    if (rmsDb < kSilenceDb)
        return false;

    // The slot: Replace fades every layer out; Add keeps up to kLayers, the oldest giving way.
    if (! settings.add)
        releaseAll();
    else
    {
        int sounding = 0, oldest = -1;
        for (int i = 0; i < (int) layers.size(); ++i)
            if (layers[(size_t) i].used && layers[(size_t) i].target > 0.0f)
            {
                ++sounding;
                if (oldest < 0 || layers[(size_t) i].order < layers[(size_t) oldest].order)
                    oldest = i;
            }
        if (sounding >= kLayers && oldest >= 0)
            layers[(size_t) oldest].target = 0.0f;
    }

    auto slot = -1;
    for (int i = 0; i < (int) layers.size(); ++i)
        if (! layers[(size_t) i].used)
        {
            slot = i;
            break;
        }
    if (slot < 0)   // every slot busy (very fast presses): take the quietest
    {
        slot = 0;
        for (int i = 1; i < (int) layers.size(); ++i)
            if (layers[(size_t) i].gain < layers[(size_t) slot].gain)
                slot = i;
    }

    auto& layer = layers[(size_t) slot];
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        const auto source = juce::jmin (ch, numChannels - 1);
        for (int i = 0; i < length; ++i)
            layer.capture[(size_t) ch][(size_t) i] = history[(size_t) source][(size_t) ((start + i) & historyMask)];
    }

    // A note: the cycle marks inside the capture, steady enough.
    std::array<int, 1024> found {};
    int numFound = 0;
    for (int k = 0; k < markCount; ++k)
    {
        const auto t = markTimes[(size_t) ((markHead - markCount + k + (int) markTimes.size()) % (int) markTimes.size())];
        if (t >= start && t < now && numFound < (int) found.size())
            found[(size_t) numFound++] = (int) (t - start);
    }

    auto isNote = false;
    auto period = 0.0f;
    if (settings.mode != Mode::texture && numFound >= kMinMarks + 1)
    {
        period = (float) (found[(size_t) numFound - 1] - found[0]) / (float) (numFound - 1);
        auto steady = true;
        for (int k = juce::jmax (1, numFound - 8); k < numFound; ++k)
            steady = steady && std::abs ((float) (found[(size_t) k] - found[(size_t) k - 1]) - period) < kStablePeriod * period;
        isNote = steady || settings.mode == Mode::note;
    }

    layer.numMarks = 0;
    if (isNote)
    {
        for (int k = 0; k < numFound; ++k)
            if ((float) found[(size_t) k] - period >= 0.0f && (float) found[(size_t) k] + period < (float) (length - 2))
                layer.marks[(size_t) layer.numMarks++] = found[(size_t) k];
        isNote = layer.numMarks >= kMinMarks;
    }

    layer.note = isNote;
    layer.period = period;
    layer.midiNote = isNote ? juce::jlimit (0, 127, (int) std::lround (69.0 + 12.0 * std::log2 (sampleRate / period / 440.0))) : -1;
    layer.lastMark = -1;
    layer.nextSequential = 0;
    layer.gain = 0.0f;
    layer.target = 1.0f;
    layer.clock = 0.0;
    layer.nextGrain = 0.0;
    for (auto& g : layer.grains)
        g.active = false;
    layer.order = addedCount;

    constexpr float pans[] = { -1.0f, 1.0f, -0.5f, 0.5f };
    layer.pan = settings.add ? settings.width * pans[addedCount % 4] : 0.0f;
    ++addedCount;
    layer.used = true;
    return true;
}

float HoldEngine::outputPeriod (const Layer& layer) const noexcept
{
    const auto ratio = settings.octave > 0 ? 2.0f : settings.octave < 0 ? 0.5f : 1.0f;
    return layer.period / ratio;
}

void HoldEngine::startGrain (Layer& layer) noexcept
{
    Grain* grain = nullptr;
    for (auto& g : layer.grains)
        if (! g.active)
        {
            grain = &g;
            break;
        }

    const auto movement = juce::jlimit (0.0f, 1.0f, settings.movement);

    if (layer.note)
    {
        // A mark: in order at Movement 0, more and more at random above it (never the same twice).
        auto index = layer.nextSequential % layer.numMarks;
        if (nextRandom() < movement && layer.numMarks > 1)
        {
            index = (int) (nextRandom() * (float) layer.numMarks) % layer.numMarks;
            if (index == layer.lastMark)
                index = (index + 1) % layer.numMarks;
        }
        layer.nextSequential = index + 1;
        layer.lastMark = index;

        if (grain != nullptr)
            *grain = { layer.clock, (double) layer.marks[(size_t) index] - layer.period, 2.0f * layer.period, 1.0f, true };

        // The next one a period on, slightly detuned with Movement.
        const auto cents = (nextRandom() * 2.0f - 1.0f) * movement * kMaxDetuneCents;
        layer.nextGrain += outputPeriod (layer) * std::exp2 (-cents / 1200.0f);
        return;
    }

    // Texture: 80 ms grains from random places, read faster or slower for the octave.
    const auto grainLength = kTextureGrainMs * 0.001f * (float) sampleRate;
    const auto rate = settings.octave > 0 ? 2.0f : settings.octave < 0 ? 0.5f : 1.0f;
    const auto span = grainLength * rate;
    const auto room = juce::jmax (0.0f, (float) layer.length - span - 2.0f);
    if (grain != nullptr)
        *grain = { layer.clock, (double) (nextRandom() * room), grainLength, rate, true };

    layer.nextGrain += 0.25 * grainLength * (1.0 + 0.5 * movement * (nextRandom() * 2.0 - 1.0));
}

float HoldEngine::readCapture (const Layer& layer, int channel, double position) const noexcept
{
    const auto& c = layer.capture[(size_t) channel];
    const auto i = juce::jlimit (0, layer.length - 2, (int) position);
    const auto t = (float) (position - (double) i);
    return c[(size_t) i] + t * (c[(size_t) i + 1] - c[(size_t) i]);
}

void HoldEngine::process (float* const* io, int channels, int numSamples) noexcept
{
    numChannels = juce::jlimit (1, kMaxChannels, channels);
    const auto ratio = settings.octave > 0 ? 2.0f : settings.octave < 0 ? 0.5f : 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // History and pitch.
        auto mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            history[(size_t) ch][(size_t) (now & historyMask)] = io[ch][i];
            mono += io[ch][i];
        }
        if (tracker.process (mono / (float) numChannels))
        {
            markTimes[(size_t) markHead] = now;
            markHead = (markHead + 1) % (int) markTimes.size();
            markCount = juce::jmin (markCount + 1, (int) markTimes.size());
        }
        ++now;

        // The held layers.
        float held[kMaxChannels] {};
        for (auto& layer : layers)
        {
            if (! layer.used)
                continue;

            layer.gain = layer.target > layer.gain ? juce::jmin (layer.target, layer.gain + attackStep)
                                                   : juce::jmax (layer.target, layer.gain - releaseStep);
            if (layer.gain <= 0.0f && layer.target <= 0.0f)
            {
                layer.used = false;
                continue;
            }

            while (layer.clock >= layer.nextGrain)
                startGrain (layer);

            // Note grains overlap 2 * ratio deep (Hann at 50% sums to 1 per pair); texture four deep.
            const auto norm = layer.note ? 1.0f / ratio : 0.5f * kTextureMakeup;
            float sum[kMaxChannels] {};
            for (auto& g : layer.grains)
            {
                if (! g.active)
                    continue;
                const auto u = (float) ((layer.clock - g.outStart) / (double) g.length);
                if (u >= 1.0f)
                {
                    g.active = false;
                    continue;
                }
                const auto w = hann.at (u);
                const auto position = g.inStart + (layer.clock - g.outStart) * g.rate;
                for (int ch = 0; ch < numChannels; ++ch)
                    sum[ch] += w * readCapture (layer, ch, position);
            }
            layer.clock += 1.0;

            const auto left = layer.gain * norm * (layer.pan > 0.0f ? 1.0f - layer.pan : 1.0f);
            const auto right = layer.gain * norm * (layer.pan < 0.0f ? 1.0f + layer.pan : 1.0f);
            if (numChannels == 2)
            {
                held[0] += left * sum[0];
                held[1] += right * sum[1];
            }
            else
            {
                held[0] += 0.5f * (left + right) * sum[0];
            }
        }

        const auto h = holdGain.getNextValue();
        const auto d = dryGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto shaped = tone[(size_t) ch].process (held[ch], rkfx::StateVariableFilter::Type::lowpass);
            io[ch][i] = io[ch][i] * d + shaped * h;
        }
    }
}
