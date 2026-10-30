#include "MixEngine.h"

namespace
{
    using FVO = juce::FloatVectorOperations;
    using mix::BusType;
    using mix::Tap;

    inline void raisePeak (float& peak, const float* data, int numSamples) noexcept
    {
        const auto range = FVO::findMinAndMax (data, numSamples);
        peak = juce::jmax (peak, std::abs (range.getStart()), std::abs (range.getEnd()));
    }

    void setValue (rkfx::Smoothed& v, float target, bool snap)
    {
        if (snap)
            v.setCurrentAndTargetValue (target);
        else
            v.setTargetValue (target);
    }

    bool isActive (const rkfx::Smoothed& v) noexcept
    {
        return v.isSmoothing() || v.getTargetValue() > 0.0f;
    }

    constexpr float kCentre = 0.70710678f;   // a mono source at the centre of a stereo bus
}

//==============================================================================
bool MixEngine::canAssign (const Settings& s, int from, int to) noexcept
{
    return to > from && s.buses[(size_t) from].type == BusType::group && s.buses[(size_t) to].type == BusType::group;
}

bool MixEngine::canSend (const Settings& s, int from, int to) noexcept
{
    if (from == to)
        return false;
    const auto source = s.buses[(size_t) from].type, dest = s.buses[(size_t) to].type;
    if (source == BusType::group)
        return dest == BusType::aux || dest == BusType::fx;
    if (source == BusType::fx)
        return dest == BusType::aux;
    return false;
}

Tap MixEngine::tapOf (int ch, int bus) const noexcept
{
    if (settings.buses[(size_t) bus].type == BusType::fx)
        return Tap::postFader;
    const auto t = settings.channels[(size_t) ch].tap[(size_t) bus];
    return t < 0 ? settings.buses[(size_t) bus].defaultTap : (Tap) juce::jlimit (0, mix::kNumTaps - 1, t);
}

//==============================================================================
void MixEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    for (auto& s : states)
    {
        s.strip.prepare (sampleRate, maxBlock);
        s.fader.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        s.mute.reset (sampleRate, kMuteFadeSeconds);
        s.pan.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        s.toMain.reset (sampleRate, kMuteFadeSeconds);
        for (auto& v : s.sends)
            v.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        for (auto& v : s.assigns)
            v.reset (sampleRate, kMuteFadeSeconds);
    }
    for (auto& b : busStates)
    {
        b.fader.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.mute.reset (sampleRate, kMuteFadeSeconds);
        b.pan.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        b.toMain.reset (sampleRate, kMuteFadeSeconds);
        for (auto& v : b.sends)
            v.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        for (auto& v : b.assigns)
            v.reset (sampleRate, kMuteFadeSeconds);
    }
    mainFader.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    mainBalance.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    monoFader.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    mainMute.reset (sampleRate, kMuteFadeSeconds);
    monoMute.reset (sampleRate, kMuteFadeSeconds);

    channelBuffer.setSize (2, maxBlock);
    scratchBuffer.setSize (2, maxBlock);
    postBuffer.setSize (2, maxBlock);
    mainBuffer.setSize (2, maxBlock);
    monoBuffer.setSize (1, maxBlock);
    keyBuffer.setSize (2, maxBlock);
    curveBuffer.setSize (numRows, maxBlock);
    busBuffers.setSize (2 * kBuses, maxBlock);

    reset();
}

void MixEngine::reset()
{
    for (int ch = 0; ch < kChannels; ++ch)
        resetChannel (ch);
    for (int bus = 0; bus < kBuses; ++bus)
        applyBus (bus, true);
    applyMain (true);
    channelPeaks.fill (0.0f);
    for (auto& p : busPeaks)
        p.fill (0.0f);
    mainPeaks.fill (0.0f);
    monoPeak = 0.0f;
}

void MixEngine::resetChannel (int ch)
{
    states[(size_t) ch].strip.setSettings (settings.channels[(size_t) ch].strip, true);
    states[(size_t) ch].strip.reset();
    applyChannel (ch, true);
}

void MixEngine::setSettings (const Settings& newSettings, std::uint64_t changed)
{
    // A pair linked or unlinked: both channels start clean (a structural change).
    std::uint64_t relinked = 0;
    for (int pair = 0; pair < kPairs; ++pair)
        if (settings.links[(size_t) pair] != newSettings.links[(size_t) pair])
            relinked |= std::uint64_t (3) << (pair * 2);

    // A bus's type or format changed: what may feed what changes everywhere.
    std::uint64_t reshaped = 0;
    for (int bus = 0; bus < kBuses; ++bus)
        if (settings.buses[(size_t) bus].type != newSettings.buses[(size_t) bus].type
            || settings.buses[(size_t) bus].stereo != newSettings.buses[(size_t) bus].stereo)
            reshaped |= busBit (bus);

    settings = newSettings;
    if (reshaped != 0)
        changed = kEverything;

    for (int ch = 0; ch < kChannels; ++ch)
    {
        const auto bit = std::uint64_t (1) << ch;
        if ((relinked & bit) != 0)
            resetChannel (ch);
        else if ((changed & bit) != 0)
            applyChannel (ch, false);
    }
    for (int bus = 0; bus < kBuses; ++bus)
        if ((changed & busBit (bus)) != 0)
            applyBus (bus, (reshaped & busBit (bus)) != 0);
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
    for (int bus = 0; bus < kBuses; ++bus)
    {
        auto& b = busStates[(size_t) bus];
        b.fader.setTargetValue (mix::faderGain (levels.busDb[(size_t) bus]));
        b.mute.setTargetValue (levels.busMute[(size_t) bus] ? 0.0f : 1.0f);
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

    s.strip.setSettings (c.strip, snap);
    setValue (s.pan, c.pan, snap);
    setValue (s.toMain, c.toMain ? 1.0f : 0.0f, snap);

    for (int bus = 0; bus < kBuses; ++bus)
    {
        const auto type = settings.buses[(size_t) bus].type;
        const auto sends = type == BusType::aux || type == BusType::fx;
        setValue (s.sends[(size_t) bus], sends ? mix::faderGain (c.sendDb[(size_t) bus]) : 0.0f, snap);
        setValue (s.assigns[(size_t) bus], type == BusType::group && c.assign[(size_t) bus] ? 1.0f : 0.0f, snap);
    }

    if (snap)
    {
        s.fader.setCurrentAndTargetValue (mix::faderGain (levels.channelDb[(size_t) ch]));
        s.mute.setCurrentAndTargetValue (levels.channelMute[(size_t) ch] ? 0.0f : 1.0f);
    }
}

void MixEngine::applyBus (int bus, bool snap)
{
    auto& b = busStates[(size_t) bus];
    const auto& s = settings.buses[(size_t) bus];

    setValue (b.pan, s.pan, snap);
    setValue (b.toMain, s.type != BusType::aux && s.toMain ? 1.0f : 0.0f, snap);
    for (int to = 0; to < kBuses; ++to)
    {
        setValue (b.sends[(size_t) to], canSend (settings, bus, to) ? mix::faderGain (s.sendDb[(size_t) to]) : 0.0f, snap);
        setValue (b.assigns[(size_t) to], canAssign (settings, bus, to) && s.assign[(size_t) to] ? 1.0f : 0.0f, snap);
    }

    if (snap)
    {
        b.fader.setCurrentAndTargetValue (mix::faderGain (levels.busDb[(size_t) bus]));
        b.mute.setCurrentAndTargetValue (levels.busMute[(size_t) bus] ? 0.0f : 1.0f);
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
MixEngine::Curve MixEngine::curveOf (rkfx::Smoothed& v, Row row, int numSamples) noexcept
{
    if (! v.isSmoothing())
        return constant (v.getTargetValue());

    auto* data = curveBuffer.getWritePointer (row);
    for (int i = 0; i < numSamples; ++i)
        data[i] = v.getNextValue();
    return { data, 0.0f };
}

void MixEngine::panCurves (rkfx::Smoothed& pan, bool stereoSource, Curve& left, Curve& right, int numSamples) noexcept
{
    const auto gains = [stereoSource] (float p, float& l, float& r)
    {
        if (stereoSource)
            mix::balanceGains (p, l, r);
        else
            mix::monoPanGains (p, l, r);
    };

    if (! pan.isSmoothing())
    {
        gains (pan.getTargetValue(), left.value, right.value);
        left.data = right.data = nullptr;
        return;
    }

    auto* l = curveBuffer.getWritePointer (panLeftRow);
    auto* r = curveBuffer.getWritePointer (panRightRow);
    for (int i = 0; i < numSamples; ++i)
        gains (pan.getNextValue(), l[i], r[i]);
    left = { l, 0.0f };
    right = { r, 0.0f };
}

void MixEngine::accumulate (float* dest, const float* source, std::initializer_list<const Curve*> gains, int numSamples) noexcept
{
    float steady = 1.0f;
    bool allSteady = true;
    for (const auto* g : gains)
    {
        if (g->steady())
            steady *= g->value;
        else
            allSteady = false;
    }

    if (allSteady)
    {
        if (juce::exactlyEqual (steady, 0.0f))
            return;
        if (juce::exactlyEqual (steady, 1.0f))
            FVO::add (dest, source, numSamples);
        else
            FVO::addWithMultiply (dest, source, steady, numSamples);
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        auto g = steady;
        for (const auto* c : gains)
            if (! c->steady())
                g *= c->data[i];
        dest[i] += source[i] * g;
    }
}

void MixEngine::addToBus (int bus, const float* const* source, int numSides, const Curve& panLeft, const Curve& panRight,
                          std::initializer_list<const Curve*> gains, int numSamples) noexcept
{
    busFed[(size_t) bus] = true;
    const auto* g = gains.begin();
    const auto one = constant (1.0f);
    const auto* g0 = gains.size() > 0 ? g[0] : &one;
    const auto* g1 = gains.size() > 1 ? g[1] : &one;
    const auto* g2 = gains.size() > 2 ? g[2] : &one;

    if (settings.buses[(size_t) bus].stereo)
    {
        accumulate (busData (bus, 0), source[0], { g0, g1, g2, &panLeft }, numSamples);
        accumulate (busData (bus, 1), source[numSides - 1], { g0, g1, g2, &panRight }, numSamples);
        return;
    }

    if (numSides == 1)
    {
        accumulate (busData (bus, 0), source[0], { g0, g1, g2 }, numSamples);
        return;
    }

    // A stereo source into a mono bus: (L + R) / 2.
    auto* mono = curveBuffer.getWritePointer (monoRow);
    FVO::add (mono, source[0], source[1], numSamples);
    const auto half = constant (0.5f);
    accumulate (busData (bus, 0), mono, { g0, g1, g2, &half }, numSamples);
}

void MixEngine::sendAtTap (int ch, const ActiveSends& active, Tap tap, const float* const* io, int numSides,
                           const Curve& mute, const Curve& fader, const Curve& panLeft, const Curve& panRight, int numSamples) noexcept
{
    auto& s = states[(size_t) ch];
    const auto centre = constant (numSides == 2 ? 1.0f : kCentre);
    const auto one = constant (1.0f);

    for (int n = 0; n < active.count; ++n)
    {
        const auto [bus, sendTap] = active.sends[(size_t) n];
        if (sendTap != tap)
            continue;

        const auto& b = settings.buses[(size_t) bus];
        const auto send = curveOf (s.sends[(size_t) bus], productRow, numSamples);
        const auto follow = b.type == BusType::fx || b.panFollows;
        addToBus (bus, io, numSides, follow ? panLeft : centre, follow ? panRight : centre,
                  { &send, &mute, tap == Tap::postFader ? &fader : &one }, numSamples);
    }
}

//==============================================================================
const float* MixEngine::keySignal (int key, int slot, const float* const* in, int numSamples) noexcept
{
    if (key <= 0 || key > kChannels)
        return nullptr;

    // The key input after its own trim (not its processing: no order to respect).
    const auto source = key - 1;
    const auto leader = mix::leaderOf (settings.links, source);
    const auto gain = mix::trimGain (settings.channels[(size_t) leader].strip.trimDb);
    auto* dest = keyBuffer.getWritePointer (slot);
    FVO::copyWithMultiply (dest, in[source], gain, numSamples);
    return dest;
}

void MixEngine::processChannel (int ch, const float* const* in, int numSamples, bool measure) noexcept
{
    auto& s = states[(size_t) ch];
    const auto& strip = settings.channels[(size_t) ch].strip;
    const auto numSides = isStereo (ch) ? 2 : 1;
    float* io[2] = { channelBuffer.getWritePointer (0), channelBuffer.getWritePointer (1) };

    const auto* gateKey = strip.gate.on ? keySignal (strip.gate.key, 0, in, numSamples) : nullptr;
    const auto* compKey = strip.comp.on ? keySignal (strip.comp.key, 1, in, numSamples) : nullptr;

    // This block's active sends (each send is read once, at its tap).
    ActiveSends active;
    for (int bus = 0; bus < kBuses; ++bus)
        if (settings.buses[(size_t) bus].type != BusType::group && isActive (s.sends[(size_t) bus]))
            active.sends[(size_t) active.count++] = { bus, tapOf (ch, bus) };

    // This block's gains (each smoother is read once).
    const auto mute = curveOf (s.mute, muteRow, numSamples);
    const auto fader = curveOf (s.fader, faderRow, numSamples);
    Curve panLeft, panRight;
    panCurves (s.pan, numSides == 2, panLeft, panRight, numSamples);

    s.strip.process (in + ch, io, numSides, numSamples, scratchBuffer, gateKey, compKey,
                     [&] (StripDsp::Tap stripTap, float* const* signal)
                     {
                         if (active.count > 0)
                             sendAtTap (ch, active, (Tap) (int) stripTap, signal, numSides, mute, fader, panLeft, panRight, numSamples);
                     });

    // (Inserts go here.)
    if (active.count > 0)
        sendAtTap (ch, active, Tap::preFader, io, numSides, mute, fader, panLeft, panRight, numSamples);

    if (measure)
        for (int side = 0; side < numSides; ++side)
            raisePeak (channelPeaks[(size_t) (ch + side)], io[side], numSamples);

    if (mute.steady() && fader.steady() && mute.value * fader.value <= 0.0f)
        return;   // silent after the fader: nothing more to add

    // Mute, fader, pan -> the main
    float* post[2] = { postBuffer.getWritePointer (0), postBuffer.getWritePointer (1) };
    for (int side = 0; side < 2; ++side)
    {
        FVO::clear (post[side], numSamples);
        accumulate (post[side], io[juce::jmin (side, numSides - 1)], { &mute, &fader, side == 0 ? &panLeft : &panRight }, numSamples);
    }

    if (isActive (s.toMain))
    {
        const auto toMain = curveOf (s.toMain, productRow, numSamples);
        for (int side = 0; side < 2; ++side)
            accumulate (mainBuffer.getWritePointer (side), post[side], { &toMain }, numSamples);
    }

    // Post-fader: the groups it is assigned to, Aux sends taken there, FX sends.
    for (int bus = 0; bus < kBuses; ++bus)
        if (settings.buses[(size_t) bus].type == BusType::group && isActive (s.assigns[(size_t) bus]))
        {
            const auto assign = curveOf (s.assigns[(size_t) bus], productRow, numSamples);
            addToBus (bus, io, numSides, panLeft, panRight, { &assign, &mute, &fader }, numSamples);
        }

    if (active.count > 0)
        sendAtTap (ch, active, Tap::postFader, io, numSides, mute, fader, panLeft, panRight, numSamples);
}

void MixEngine::processBus (int bus, float* const* out, int numSamples, bool measure) noexcept
{
    auto& b = busStates[(size_t) bus];
    const auto& s = settings.buses[(size_t) bus];
    const auto numSides = s.stereo ? 2 : 1;
    float* io[2] = { busData (bus, 0), busData (bus, numSides - 1) };

    // Nothing fed it and nothing glides: silent, nothing to do.
    if (! busFed[(size_t) bus] && ! b.fader.isSmoothing() && ! b.mute.isSmoothing())
        return;

    // (Inserts go here.)

    // Mute and fader in place: the bus's output signal.
    const auto mute = curveOf (b.mute, muteRow, numSamples);
    const auto fader = curveOf (b.fader, faderRow, numSamples);
    for (int side = 0; side < numSides; ++side)
    {
        if (mute.steady() && fader.steady())
        {
            const auto g = mute.value * fader.value;
            if (! juce::exactlyEqual (g, 1.0f))
                FVO::multiply (io[side], g, numSamples);
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
                io[side][i] *= mute.at (i) * fader.at (i);
        }
    }

    if (measure)
        for (int side = 0; side < numSides; ++side)
            raisePeak (busPeaks[(size_t) bus][(size_t) side], io[side], numSamples);

    addToTarget (out, s.output, io[0], numSides == 2 ? io[1] : nullptr, numSamples);

    if (s.type == BusType::aux)
        return;

    Curve panLeft, panRight;
    panCurves (b.pan, numSides == 2, panLeft, panRight, numSamples);

    if (isActive (b.toMain))
    {
        const auto toMain = curveOf (b.toMain, productRow, numSamples);
        accumulate (mainBuffer.getWritePointer (0), io[0], { &toMain, &panLeft }, numSamples);
        accumulate (mainBuffer.getWritePointer (1), io[numSides - 1], { &toMain, &panRight }, numSamples);
    }

    for (int to = 0; to < kBuses; ++to)
    {
        if (isActive (b.assigns[(size_t) to]))
        {
            const auto assign = curveOf (b.assigns[(size_t) to], productRow, numSamples);
            addToBus (to, io, numSides, panLeft, panRight, { &assign }, numSamples);
        }
        if (isActive (b.sends[(size_t) to]))
        {
            const auto send = curveOf (b.sends[(size_t) to], productRow, numSamples);
            addToBus (to, io, numSides, panLeft, panRight, { &send }, numSamples);
        }
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
    busBuffers.clear (0, numSamples);
    busFed.fill (false);

    for (int ch = 0; ch < kChannels; ++ch)
        if (! isFollower (ch))
            processChannel (ch, in, numSamples, measure);

    // Groups in number order (a group feeds only later groups), then FX, then Aux.
    for (const auto type : { BusType::group, BusType::fx, BusType::aux })
        for (int bus = 0; bus < kBuses; ++bus)
            if (settings.buses[(size_t) bus].type == type)
                processBus (bus, out, numSamples, measure);

    processMain (out, numSamples, measure);
}
