#include "PlayEngine.h"

#include <cmath>

namespace
{
    // The fade curve: a quarter sine, so a crossfade keeps the power constant.
    inline float curve (double fade) noexcept
    {
        return (float) std::sin (fade * juce::MathConstants<double>::halfPi);
    }
}

void PlayEngine::prepare (double rate, int maxBlock)
{
    sampleRate = rate;
    scratch.setSize (2, maxBlock);
    level.reset (sampleRate, 0.05);
    level.setCurrentAndTargetValue (level.getTargetValue());
    for (auto& v : voices)
        v = {};
    current = -1;
    armedSlot = -1;
    pauseFade = 1.0;
    pauseStep = 0.0;
    paused = false;
    currentSlot = -1;
    pausedState = false;
    for (auto& b : busy)
        b = false;
}

//==============================================================================
void PlayEngine::push (const Command& c)
{
    int start1, size1, start2, size2;
    commandFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 + size2 == 0)
    {
        jassertfalse;   // the audio thread isn't running: commands pile up
        return;
    }
    commands[(size_t) (size1 > 0 ? start1 : start2)] = c;
    commandFifo.finishedWrite (1);
    ++sent;
}

void PlayEngine::start (int slot, double seconds, float gain)       { push ({ Command::Type::start, slot, seconds, gain }); }
void PlayEngine::crossfadeTo (int slot, double seconds, float gain) { push ({ Command::Type::crossfade, slot, seconds, gain }); }
void PlayEngine::arm (int slot, double seconds, float gain)         { push ({ Command::Type::arm, slot, seconds, gain }); }
void PlayEngine::disarm()                                           { push ({ Command::Type::disarm, -1, 0.0, 1.0f }); }
void PlayEngine::fadeOutAll (double seconds)                        { push ({ Command::Type::fadeOutAll, -1, seconds, 1.0f }); }
void PlayEngine::pause (double seconds)                             { push ({ Command::Type::pause, -1, seconds, 1.0f }); }
void PlayEngine::resume (double seconds)                            { push ({ Command::Type::resume, -1, seconds, 1.0f }); }

void PlayEngine::post (Event::Type type, int slot) noexcept
{
    int start1, size1, start2, size2;
    eventFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 + size2 == 0)
        return;   // nobody reads them (no message thread): drop
    events[(size_t) (size1 > 0 ? start1 : start2)] = { type, slot };
    eventFifo.finishedWrite (1);
}

bool PlayEngine::popEvent (Event& e)
{
    int start1, size1, start2, size2;
    eventFifo.prepareToRead (1, start1, size1, start2, size2);
    if (size1 + size2 == 0)
        return false;
    e = events[(size_t) (size1 > 0 ? start1 : start2)];
    eventFifo.finishedRead (1);
    return true;
}

//==============================================================================
int PlayEngine::startVoice (int slot, double seconds, float gain) noexcept
{
    if (slot < 0 || slot >= kSlots || streams[(size_t) slot] == nullptr)
        return -1;

    // A free voice, or else the one fading out the furthest along.
    int index = -1;
    for (int v = 0; v < kVoices; ++v)
        if (! voices[(size_t) v].active)
        {
            index = v;
            break;
        }
    if (index < 0)
    {
        double quietest = 2.0;
        for (int v = 0; v < kVoices; ++v)
            if (v != current && voices[(size_t) v].fade < quietest)
            {
                quietest = voices[(size_t) v].fade;
                index = v;
            }
        release (index);
    }

    auto& voice = voices[(size_t) index];
    voice.active = true;
    voice.slot = slot;
    voice.gain = gain;
    voice.releasing = false;
    voice.fade = seconds > 0.0 ? 0.0 : 1.0;
    voice.fadeStep = seconds > 0.0 ? stepFor (seconds) : 0.0;
    return index;
}

void PlayEngine::fadeOut (Voice& voice, double seconds) noexcept
{
    if (! voice.active)
        return;
    voice.releasing = true;
    voice.fadeStep = -stepFor (seconds);
}

void PlayEngine::release (int index) noexcept
{
    if (index < 0)
        return;
    auto& voice = voices[(size_t) index];
    if (! voice.active)
        return;
    voice.active = false;
    post (Event::Type::released, voice.slot);
    if (index == current)
    {
        current = -1;
        currentSlot = -1;
        post (Event::Type::ended, voice.slot);
    }
}

void PlayEngine::handle (const Command& c) noexcept
{
    switch (c.type)
    {
        case Command::Type::start:
        case Command::Type::crossfade:
        {
            // Paused: the held songs go at once (silent anyway); a pause still
            // fading turns back into the new song's fade in.
            if (paused)
            {
                for (int v = 0; v < kVoices; ++v)
                    release (v);
                paused = false;
                pauseFade = 1.0;
                pauseStep = 0.0;
            }
            else if (pauseStep < 0.0)
            {
                pauseStep = stepFor (juce::jmax (0.05, c.seconds));
            }
            if (c.type == Command::Type::crossfade)
                for (auto& v : voices)
                    fadeOut (v, c.seconds);
            armedSlot = -1;
            const auto v = startVoice (c.slot, c.seconds, c.gain);
            if (v >= 0)
            {
                current = v;
                currentSlot = c.slot;
            }
            break;
        }
        case Command::Type::arm:
            armedSlot = c.slot;
            armedSeconds = c.seconds;
            armedGain = c.gain;
            break;
        case Command::Type::disarm:
            armedSlot = -1;
            break;
        case Command::Type::fadeOutAll:
            if (paused)
            {
                // Held and silent: let them go now, and be ready to play again.
                for (int v = 0; v < kVoices; ++v)
                    release (v);
                paused = false;
                pauseFade = 1.0;
                pauseStep = 0.0;
            }
            for (auto& v : voices)
                fadeOut (v, c.seconds);
            armedSlot = -1;
            break;
        case Command::Type::pause:
            if (c.seconds <= 0.0)
            {
                pauseFade = 0.0;
                pauseStep = 0.0;
                paused = true;
            }
            else if (! paused)
            {
                pauseStep = -stepFor (c.seconds);
            }
            break;
        case Command::Type::resume:
            paused = false;
            pauseStep = stepFor (c.seconds);
            break;
    }
    pausedState = paused || pauseStep < 0.0;
}

juce::int64 PlayEngine::framesLeft (const Voice& voice) const noexcept
{
    const auto* stream = streams[(size_t) voice.slot];
    return stream->isSourceDone() ? (juce::int64) stream->available() : stream->framesLeft();
}

void PlayEngine::advance() noexcept
{
    const auto slot = armedSlot;
    armedSlot = -1;
    const auto old = current;
    const auto left = old >= 0 ? (double) framesLeft (voices[(size_t) old]) / sampleRate : 0.0;

    const auto v = startVoice (slot, armedSeconds, armedGain);
    if (v < 0)
        return;
    // Gapless (no crossfade): the old song is at its last sample; a 2 ms fade
    // only matters if the length estimate was a few samples short.
    if (old >= 0)
        fadeOut (voices[(size_t) old], armedSeconds > 0.0 ? juce::jmax (0.001, juce::jmin (armedSeconds, left)) : 0.002);
    current = v;
    currentSlot = slot;
    post (Event::Type::advanced, slot);
}

//==============================================================================
void PlayEngine::render (float* const* out, int offset, int numSamples) noexcept
{
    if (paused)
        return;   // held: nothing read, nothing heard

    const auto underrunFrames = (int) (kUnderrunSeconds * sampleRate);
    float* tmp[2] = { scratch.getWritePointer (0), scratch.getWritePointer (1) };

    for (int index = 0; index < kVoices; ++index)
    {
        auto& voice = voices[(size_t) index];
        if (! voice.active)
            continue;

        auto* stream = streams[(size_t) voice.slot];
        const auto got = stream->read (tmp, 0, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            if (got < numSamples)
                juce::FloatVectorOperations::clear (tmp[ch] + got, numSamples - got);

        if (juce::exactlyEqual (voice.fadeStep, 0.0))
        {
            const auto g = voice.gain * curve (voice.fade);
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::addWithMultiply (out[ch] + offset, tmp[ch], g, numSamples);
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto g = voice.gain * curve (voice.fade);
                out[0][offset + i] += tmp[0][i] * g;
                out[1][offset + i] += tmp[1][i] * g;
                voice.fade = juce::jlimit (0.0, 1.0, voice.fade + voice.fadeStep);
            }
            if (voice.fade >= 1.0 && voice.fadeStep > 0.0)
                voice.fadeStep = 0.0;
        }

        // Done: faded out, or the song's last sample played (unless the next
        // song is armed to follow it: process() hands over first).
        const auto handingOver = index == current && armedSlot >= 0 && got == numSamples;
        if ((voice.releasing && voice.fade <= 0.0) || (got < numSamples && stream->isSourceDone())
            || (stream->isSourceDone() && stream->available() == 0 && ! handingOver))
        {
            release (index);
            continue;
        }

        // Running dry before its end (a slow disk): fade out while there is still sound to fade.
        if (! stream->isSourceDone() && stream->available() < underrunFrames && ! voice.releasing)
        {
            fadeOut (voice, kUnderrunFade);
            post (Event::Type::underrun, voice.slot);
        }
    }

    // Pause and resume fade everything together.
    if (pauseStep != 0.0 || pauseFade < 1.0)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = curve (pauseFade);
            out[0][offset + i] *= g;
            out[1][offset + i] *= g;
            pauseFade = juce::jlimit (0.0, 1.0, pauseFade + pauseStep);
        }
        if (pauseFade <= 0.0 && pauseStep < 0.0)
        {
            pauseStep = 0.0;
            paused = true;
        }
        else if (pauseFade >= 1.0 && pauseStep > 0.0)
        {
            pauseStep = 0.0;
        }
        pausedState = paused || pauseStep < 0.0;
    }
}

void PlayEngine::process (float* const* out, int numSamples) noexcept
{
    {
        int start1, size1, start2, size2;
        const auto ready = commandFifo.getNumReady();
        commandFifo.prepareToRead (ready, start1, size1, start2, size2);
        for (int i = 0; i < size1; ++i)
            handle (commands[(size_t) (start1 + i)]);
        for (int i = 0; i < size2; ++i)
            handle (commands[(size_t) (start2 + i)]);
        commandFifo.finishedRead (size1 + size2);
        handled += (juce::uint32) (size1 + size2);
    }

    // In pieces, so the next song starts exactly when the current one has its crossfade left.
    int pos = 0;
    while (pos < numSamples)
    {
        auto chunk = numSamples - pos;
        if (armedSlot >= 0 && current >= 0 && ! paused && ! voices[(size_t) current].releasing)
        {
            const auto until = framesLeft (voices[(size_t) current]) - (juce::int64) std::llround (armedSeconds * sampleRate);
            if (until <= 0)
            {
                advance();
                continue;
            }
            chunk = (int) juce::jmin<juce::int64> (chunk, until);
        }
        render (out, pos, chunk);
        pos += chunk;
    }

    // Which streams are in use (the message thread reuses only idle ones).
    for (int slot = 0; slot < kSlots; ++slot)
    {
        auto inUse = slot == armedSlot;
        for (const auto& v : voices)
            inUse = inUse || (v.active && v.slot == slot);
        busy[(size_t) slot] = inUse;
    }

    if (level.isSmoothing() || ! juce::exactlyEqual (level.getTargetValue(), 1.0f))
        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = level.getNextValue();
            out[0][i] *= g;
            out[1][i] *= g;
        }
}
