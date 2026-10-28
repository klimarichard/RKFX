#include "VoiceEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Detune per voice, as a share of Detune: the voices spread around the
    // exact pitch rather than all sharp.
    constexpr std::array<float, VoiceEngine::kNumVoices> kDetuneShare { 1.0f, -1.0f, 0.5f, -0.5f };

    float dbToGain (float db) noexcept { return db <= -60.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
}

int VoiceEngine::chordNote (int sung, int position, const NoteSet& chord, ChordTones tones) noexcept
{
    if (position == 0 || sung < 0 || sung > 127)
        return -1;

    const auto step = position > 0 ? 1 : -1;
    auto count = std::abs (position);

    if (tones == ChordTones::anyOctave)
    {
        std::array<bool, 12> classes {};
        bool any = false;
        for (int n = 0; n < 128; ++n)
            if (chord[(size_t) n])
                any = classes[(size_t) (n % 12)] = true;
        if (! any)
            return -1;

        for (int n = sung + step; n >= 0 && n <= 127; n += step)
            if (classes[(size_t) (n % 12)] && --count == 0)
                return n;
        return -1;
    }

    for (int n = sung + step; n >= 0 && n <= 127; n += step)
        if (chord[(size_t) n] && --count == 0)
            return n;
    return -1;
}

float VoiceEngine::shiftFor (int sung, int target) noexcept
{
    return (float) std::clamp (target - sung, -kMaxShift, kMaxShift);
}

void VoiceEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    // History: two periods of the lowest note any range tracks, plus the
    // latency (up to two more periods) and room to spare.
    auto size = 1;
    while (size < (int) (sampleRate * 0.3))
        size *= 2;
    history.assign ((size_t) size, 0.0f);
    historyMask = size - 1;

    for (size_t i = 0; i < hann.size(); ++i)
        hann[i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (hann.size() - 1));

    tracker.prepare (sampleRate);
    inputLevel.prepare (sampleRate);
    dryGain.reset (sampleRate, rkfx::kParamSmoothingSeconds);

    for (size_t v = 0; v < voices.size(); ++v)
    {
        auto& voice = voices[v];
        voice.humanize.prepare ((int) std::ceil (sampleRate * (kMaxHumanizeMs + 2.0f) / 1000.0));
        voice.wander = rkfx::RandomModulator (0x9e3779b9u * (std::uint32_t) (v + 1));
        voice.wander.prepare (sampleRate);
        voice.wander.setRate (0.6f + 0.15f * (float) v);
        voice.gainL.reset (sampleRate, rkfx::kParamSmoothingSeconds);
        voice.gainR.reset (sampleRate, rkfx::kParamSmoothingSeconds);
    }

    fadeStep = (float) (1000.0 / (kFadeMs * sampleRate));
    setSettings (settings);
    reset();
}

void VoiceEngine::reset()
{
    std::fill (history.begin(), history.end(), 0.0f);
    now = 0;
    tracker.setRange (settings.range);
    tracker.reset();
    inputLevel.reset();
    gateOpen = pitchValid = false;
    markTime = lastCycle = -1;
    sinceMark = 0;
    markPeriod = markHz = lastInterval = 0.0f;
    steadyCount = 0;
    sungNote = pendingNote = -1;
    pendingSince = 0;

    for (auto& voice : voices)
    {
        voice.target = -1;
        voice.shiftSet = false;
        voice.fade = 0.0f;
        voice.nextOut = -1.0;
        voice.numGrains = 0;
        voice.humanize.reset();
        voice.wander.reset();
        voice.gainL.setCurrentAndTargetValue (voice.gainL.getTargetValue());
        voice.gainR.setCurrentAndTargetValue (voice.gainR.getTargetValue());
    }
    dryGain.setCurrentAndTargetValue (dryGain.getTargetValue());
    targetsDirty = true;
    sungShown.store (-1);
    for (auto& shown : voiceShown)
        shown.store (-1);
}

void VoiceEngine::setSettings (const Settings& s)
{
    const auto rangeChanged = s.range != settings.range;
    settings = s;

    if (rangeChanged)
    {
        tracker.setRange (settings.range);
        pitchValid = false;
        sungNote = -1;
    }

    // The threshold detector releases over about 2.5 periods of the range's
    // lowest note (as RKSynth), so it rides over the cycles of low notes.
    inputLevel.setTimes (1.0f, std::max (10.0f, 2500.0f / rkfx::PitchTracker::limitsFor (settings.range).lowHz));

    dryGain.setTargetValue (dbToGain (settings.dryDb));
    for (size_t v = 0; v < voices.size(); ++v)
    {
        const auto& vs = settings.voices[v];
        const auto g = dbToGain (vs.levelDb);
        const auto pan = std::clamp (vs.pan, -1.0f, 1.0f);
        voices[v].gainL.setTargetValue (g * (pan > 0.0f ? 1.0f - pan : 1.0f));
        voices[v].gainR.setTargetValue (g * (pan < 0.0f ? 1.0f + pan : 1.0f));
    }

    // Glide: 95% of the way to a new note in the Glide time.
    const auto glideSamples = std::max (1.0, settings.glideMs * 0.001 * sampleRate);
    glideCoef = (float) std::exp (-3.0 / glideSamples);
    targetsDirty = true;
}

//==============================================================================
void VoiceEngine::noteOn (int note) noexcept
{
    if (note < 0 || note > 127)
        return;
    ++held[(size_t) note];
    sustained[(size_t) note] = false;
    targetsDirty = true;
}

void VoiceEngine::noteOff (int note) noexcept
{
    if (note < 0 || note > 127 || held[(size_t) note] == 0)
        return;
    if (--held[(size_t) note] == 0 && sustainDown)
        sustained[(size_t) note] = true;
    targetsDirty = true;
}

void VoiceEngine::setSustain (bool down) noexcept
{
    if (down == sustainDown)
        return;
    sustainDown = down;
    if (! down)
        sustained.fill (false);
    targetsDirty = true;
}

void VoiceEngine::allNotesOff() noexcept
{
    held.fill (0);
    sustained.fill (false);
    targetsDirty = true;
}

VoiceEngine::NoteSet VoiceEngine::chordInUse (bool& hasChord) const noexcept
{
    NoteSet chord {};
    hasChord = false;
    for (size_t n = 0; n < chord.size(); ++n)
        if (held[n] > 0 || sustained[n])
            hasChord = chord[n] = true;
    return chord;
}

void VoiceEngine::updateTargets() noexcept
{
    targetsDirty = false;

    bool hasChord = false;
    auto chord = chordInUse (hasChord);
    if (hasChord)
        lastChord = chord;

    // Without a chord: silent, the last chord, or the scale.
    auto useScale = settings.mode == Mode::scale;
    if (settings.mode == Mode::chord && ! hasChord)
    {
        if (settings.noChord == NoChord::latch)
            chord = lastChord;
        else if (settings.noChord == NoChord::scale)
            useScale = true;
    }

    int classes = 0;
    if (! useScale)
        for (int n = 0; n < 128; ++n)
            if (chord[(size_t) n])
                classes |= 1 << (n % 12);
    chordShown.store (classes, std::memory_order_relaxed);

    for (size_t v = 0; v < voices.size(); ++v)
    {
        const auto& vs = settings.voices[v];
        auto& voice = voices[v];
        voice.target = sungNote < 0 ? -1
                     : useScale ? rkfx::scales::harmonyNote (sungNote, settings.key, settings.scale, vs.interval)
                                : chordNote (sungNote, vs.position, chord, settings.chordTones);
        if (voice.target >= 0 && std::abs (voice.target - sungNote) > kMaxShift)
            voice.target = -1;
    }
}

//==============================================================================
void VoiceEngine::trackSample (float x) noexcept
{
    // The threshold, with 3 dB of hysteresis; below it, the note is over.
    const auto threshold = std::pow (10.0f, settings.thresholdDb * 0.05f);
    const auto level = inputLevel.process (x);
    if (! gateOpen && level > threshold)
        gateOpen = true;
    else if (gateOpen && level < threshold * 0.7079f)
    {
        gateOpen = false;
        tracker.reset();
        pitchValid = false;
        markTime = lastCycle = -1;
        lastInterval = 0.0f;
        steadyCount = 0;
        if (sungNote >= 0)
        {
            sungNote = pendingNote = -1;
            targetsDirty = true;
        }
    }

    // No steady cycle for 2.5 periods of the sung note (one missed crossing
    // is a gap of two): the note has stopped, so the voices fade rather than
    // repeat its last period.
    ++sinceMark;
    if (pitchValid && (float) sinceMark > 2.5f * markPeriod)
        pitchValid = false;

    if (! tracker.process (gateOpen ? x : 0.0f) || ! gateOpen)
        return;

    // The period: the time since the last cycle, trusted when the one before
    // agrees (the first cycle of a note, or a missed or doubled crossing, don't).
    const auto interval = lastCycle >= 0 ? (float) (now - lastCycle) : 0.0f;
    lastCycle = now;
    const auto limits = rkfx::PitchTracker::limitsFor (settings.range);
    const auto inRange = interval > 0.0f && interval >= (float) sampleRate / (limits.highHz * 1.5f)
                                         && interval <= (float) sampleRate / (limits.lowHz * 0.5f);
    const auto stable = inRange && lastInterval > 0.0f && std::abs (interval - lastInterval) < kStablePeriod * interval;
    lastInterval = inRange ? interval : 0.0f;
    steadyCount = stable ? steadyCount + 1 : 0;
    if (! stable || (! pitchValid && steadyCount < kOnsetPeriods - 1))
        return;

    const auto hz = (float) (sampleRate / interval);
    markTime = now;
    markHz = hz;
    markPeriod = interval;
    sinceMark = 0;
    pitchValid = true;

    // The sung note, with hysteresis.
    const auto midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const auto nearest = (int) std::lround (midi);
    if (sungNote < 0)
    {
        sungNote = std::clamp (nearest, 0, 127);
        pendingNote = -1;
        targetsDirty = true;
    }
    else if (std::abs (midi - (float) sungNote) > kHoldRangeSemitones)
    {
        sungNote = std::clamp (nearest, 0, 127);   // a new note
        pendingNote = -1;
        targetsDirty = true;
    }
    else if (std::abs (midi - (float) sungNote) * 100.0f > kHysteresisCents)
    {
        if (nearest != pendingNote)
        {
            pendingNote = nearest;
            pendingSince = now;
        }
        else if ((double) (now - pendingSince) >= kHoldMs * 0.001 * sampleRate)
        {
            sungNote = std::clamp (nearest, 0, 127);
            pendingNote = -1;
            targetsDirty = true;
        }
    }
    else
    {
        pendingNote = -1;
    }
}

float VoiceEngine::readHistory (double t) const noexcept
{
    const auto whole = (std::int64_t) std::floor (t);
    const auto frac = (float) (t - (double) whole);
    const auto a = history[(size_t) (whole & historyMask)];
    if (whole + 1 > now)
        return a;
    const auto b = history[(size_t) ((whole + 1) & historyMask)];
    return a + frac * (b - a);
}

float VoiceEngine::window (float position) const noexcept
{
    const auto x = (position + 1.0f) * 0.5f * (float) (hann.size() - 1);
    const auto i = std::clamp ((int) x, 0, (int) hann.size() - 2);
    const auto t = x - (float) i;
    return hann[(size_t) i] + t * (hann[(size_t) i + 1] - hann[(size_t) i]);
}

float VoiceEngine::voiceSample (Voice& voice, int index) noexcept
{
    // Fade in while there's a pitch and a note to sing; fade out otherwise.
    const auto singing = pitchValid && voice.target >= 0 && sungNote >= 0;
    voice.fade = singing ? std::min (1.0f, voice.fade + fadeStep) : std::max (0.0f, voice.fade - fadeStep);

    if (singing)
    {
        // The interval glides; the detune spreads the voices a little.
        const auto wanted = shiftFor (sungNote, voice.target) + settings.detuneCents * 0.01f * kDetuneShare[(size_t) index];
        voice.shift = voice.shiftSet ? wanted + glideCoef * (voice.shift - wanted) : wanted;
        voice.shiftSet = true;
        const auto ratio = std::exp2 (voice.shift / 12.0f);
        const auto outPeriod = (double) (markPeriod / ratio);

        // New grains: each centred one input period after its mark, so the
        // input it reads has always arrived.
        if (voice.nextOut < 0.0 || voice.nextOut < (double) now - 4.0 * markPeriod)
            voice.nextOut = (double) markTime + markPeriod;
        while ((double) now >= voice.nextOut - markPeriod)
        {
            // A grain reads the input at (its mark + the time since its centre),
            // which has arrived as long as the centre isn't before the mark.
            if (voice.numGrains < kMaxGrains)
                voice.grains[(size_t) voice.numGrains++] = { (double) markTime, std::max (voice.nextOut, (double) markTime), markPeriod };
            voice.nextOut += std::max (outPeriod, 1.0);
        }
    }
    else if (voice.fade <= 0.0f)
    {
        voice.numGrains = 0;
        voice.nextOut = -1.0;
        voice.shiftSet = false;
    }

    // Overlap-add the grains, normalised by the windows' sum so the level
    // stays even whatever the shift.
    float sum = 0.0f, weight = 0.0f;
    for (int g = 0; g < voice.numGrains;)
    {
        auto& grain = voice.grains[(size_t) g];
        const auto d = (double) now - grain.outCentre;
        if (d >= grain.half)
        {
            grain = voice.grains[(size_t) --voice.numGrains];   // finished
            continue;
        }
        if (d > -grain.half)
        {
            const auto w = window ((float) (d / grain.half));
            sum += w * readHistory (grain.inCentre + d);
            weight += w;
        }
        ++g;
    }
    return voice.fade * sum / std::max (weight, 0.5f);
}

void VoiceEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0)
        return;
    const auto stereo = numChannels > 1;
    auto* left = io[0];
    auto* right = io[stereo ? 1 : 0];
    const auto humanizeSamples = std::clamp (settings.humanizeMs, 0.0f, kMaxHumanizeMs) * 0.001f * (float) sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto inL = left[i];
        const auto inR = stereo ? right[i] : inL;
        const auto mono = stereo ? 0.5f * (inL + inR) : inL;

        history[(size_t) (now & historyMask)] = mono;
        trackSample (mono);
        if (targetsDirty)
            updateTargets();

        float wetL = 0.0f, wetR = 0.0f;
        for (size_t v = 0; v < voices.size(); ++v)
        {
            auto& voice = voices[v];
            auto y = voiceSample (voice, (int) v);
            if (humanizeSamples > 0.0f)
            {
                // A few ms of slowly wandering delay, so the voices don't sit locked together.
                voice.humanize.write (y);
                y = voice.humanize.read (1.0f + humanizeSamples * (0.5f + 0.5f * voice.wander.next()));
            }
            wetL += y * voice.gainL.getNextValue();
            wetR += y * voice.gainR.getNextValue();
        }

        const auto dry = dryGain.getNextValue();
        if (stereo)
        {
            left[i] = inL * dry + wetL;
            right[i] = inR * dry + wetR;
        }
        else
        {
            left[i] = inL * dry + 0.5f * (wetL + wetR);
        }
        ++now;
    }

    sungShown.store (sungNote, std::memory_order_relaxed);
    for (size_t v = 0; v < voices.size(); ++v)
        voiceShown[v].store (voices[v].fade > 0.0f ? voices[v].target : -1, std::memory_order_relaxed);
}
