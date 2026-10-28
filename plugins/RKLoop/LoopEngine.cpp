#include "LoopEngine.h"

#include <cmath>

int LoopEngine::syncedLength (int recorded, int master, int maxLength) noexcept
{
    if (master <= 0)
        return juce::jmin (recorded, maxLength);

    // 1/4, 1/2, then whole multiples, as long as they fit.
    auto best = master / 4;
    const auto consider = [&] (int candidate)
    {
        if (candidate > 0 && candidate <= maxLength && std::abs (candidate - recorded) < std::abs (best - recorded))
            best = candidate;
    };
    consider (master / 2);
    for (int k = 1; k * (long long) master <= maxLength; ++k)
        consider (k * master);
    return best;
}

int LoopEngine::getMasterLength() const noexcept
{
    const auto& first = loops[0];
    return sync && first.state != State::recording ? first.length.load() : 0;
}

void LoopEngine::prepare (double newSampleRate)
{
    fadeLength = juce::jmax (1, (int) (kFadeMs * 0.001 * newSampleRate));
    for (auto& l : loops)
        l.gain.reset (newSampleRate, kFadeMs * 0.001);
    level.reset (newSampleRate, rkfx::kParamSmoothingSeconds);

    // Same rate: the loops stay (hosts prepare again for a new block size).
    if (juce::exactlyEqual (newSampleRate, sampleRate) && maxLength > 0)
        return;

    sampleRate = newSampleRate;
    maxLength = (int) (kMaxSeconds * sampleRate);
    for (auto& l : loops)
    {
        l.audio.setSize (kMaxChannels, maxLength, false, false, false);
        l.undo.setSize (kMaxChannels, maxLength, false, false, false);
        l.state = State::empty;
        l.length = l.written = 0;
        l.queued = false;
        l.queuedRecord = false;
        l.recordTarget = -1;
        l.undoCount = 0;
        l.undone = false;
        l.swapCursor = -1;
        l.position = 0.0;
        l.shownPosition = 0.0;
        l.gain.setCurrentAndTargetValue (0.0f);
        l.afterFade = AfterFade::nothing;
    }
    clock = 0.0;
    clockRunning = false;
    currentPart = pendingPart = -1;
}

void LoopEngine::reset()
{
    for (int i = 0; i < kLoops; ++i)
    {
        auto& l = loops[(size_t) i];
        if (l.state == State::recording)
        {
            closeRecording (i, l.written);
            if (l.length > 0)
                l.state = State::stopped;
        }
        if (l.state == State::playing || l.state == State::overdubbing)
            l.state = State::stopped;
        l.queued = false;
        l.queuedRecord = false;
        l.gain.setCurrentAndTargetValue (0.0f);
        l.afterFade = AfterFade::nothing;
        l.position = 0.0;
    }
    clock = 0.0;
    clockRunning = false;
    currentPart = pendingPart = -1;
}

bool LoopEngine::isRunning (int i) const noexcept
{
    const auto s = loops[(size_t) i].state.load();
    return s == State::recording || s == State::playing || s == State::overdubbing;
}

bool LoopEngine::anyRunning() const noexcept
{
    for (int i = 0; i < kLoops; ++i)
        if (isRunning (i))
            return true;
    return false;
}

bool LoopEngine::startsOnDownbeat (int) const noexcept
{
    return getMasterLength() > 0 && clockRunning;
}

//==============================================================================
void LoopEngine::press (Button button) noexcept
{
    if (maxLength == 0)
        return;

    const auto i = selected.load();
    auto& l = loops[(size_t) i];

    switch (button)
    {
        case Button::next:
            select ((i + 1) % kLoops);
            break;

        case Button::previous:
            select ((i + kLoops - 1) % kLoops);
            break;

        case Button::record:
            switch (l.state.load())
            {
                case State::empty:
                    if (l.queued)
                        l.queued = l.queuedRecord = false;    // cancel
                    else
                        requestRecording (i);
                    break;
                case State::recording:
                    if (l.recordTarget < 0)
                        endRecording (i, true);
                    break;
                case State::playing:
                    if (! halfSpeed && l.swapCursor < 0 && l.afterFade == AfterFade::nothing)
                        startOverdub (i);
                    break;
                case State::overdubbing:
                    l.state = State::playing;
                    break;
                case State::stopped:
                    if (l.queued)
                    {
                        l.queued = false;                     // cancel
                        if (pendingPart == i)
                            pendingPart = -1;
                    }
                    else
                    {
                        requestPlaying (i);
                    }
                    break;
            }
            break;

        case Button::stop:
            if (l.state == State::recording)
                endRecording (i, false);

            if (l.role == Role::part)
            {
                // A part: the whole song stops.
                for (int j = 0; j < kLoops; ++j)
                {
                    auto& other = loops[(size_t) j];
                    other.queued = other.queuedRecord = false;
                    if (other.state == State::playing || other.state == State::overdubbing)
                        fadeOut (j, AfterFade::stop);
                }
                pendingPart = -1;
            }
            else
            {
                l.queued = l.queuedRecord = false;
                if (l.state == State::playing || l.state == State::overdubbing)
                    fadeOut (i, AfterFade::stop);
            }
            break;

        case Button::undo:
            if (l.state == State::overdubbing)
                l.state = State::playing;
            if (l.undoCount > 0 && l.swapCursor < 0 && (l.state == State::playing || l.state == State::stopped))
                l.swapCursor = 0;
            break;

        case Button::clear:
            l.queued = l.queuedRecord = false;
            if (pendingPart == i)
                pendingPart = -1;

            if (l.state == State::recording)
            {
                l.state = State::empty;
                l.length = l.written = 0;
                l.recordTarget = -1;
            }
            else if (l.state == State::playing || l.state == State::overdubbing)
            {
                fadeOut (i, AfterFade::clear);
            }
            else if (l.state == State::stopped)
            {
                l.state = State::empty;
                l.length = 0;
                l.undoCount = 0;
                l.undone = false;
            }
            break;
    }
}

void LoopEngine::select (int index) noexcept
{
    index = juce::jlimit (0, kLoops - 1, index);
    selected = index;

    // Choosing a recorded part while another part plays: switch at its end.
    const auto& l = loops[(size_t) index];
    if (l.role != Role::part || currentPart < 0 || ! isRunning (currentPart))
        return;

    if (pendingPart >= 0 && pendingPart != index)
        loops[(size_t) pendingPart].queued = false;

    if (index == currentPart)
    {
        pendingPart = -1;                                  // stay: cancels a queued switch
    }
    else if (l.state == State::stopped && l.length > 0)
    {
        pendingPart = index;
        loops[(size_t) index].queued = true;
    }
}

void LoopEngine::setRole (int index, Role role) noexcept
{
    auto& l = loops[(size_t) index];
    if (l.role == role)
        return;

    l.role = role;
    if (role == Role::part && isRunning (index))
    {
        stopOtherParts (index);
        currentPart = index;
    }
    else if (role == Role::layer)
    {
        if (currentPart == index)
            currentPart = -1;
        if (pendingPart == index)
        {
            pendingPart = -1;
            l.queued = false;
        }
    }
}

//==============================================================================
void LoopEngine::beginRecording (int i) noexcept
{
    auto& l = loops[(size_t) i];
    if (l.role == Role::part)
    {
        stopOtherParts (i);
        currentPart = i;
    }

    l.written = 0;
    l.recordTarget = -1;
    l.queued = l.queuedRecord = false;
    l.state = State::recording;
}

void LoopEngine::endRecording (int i, bool thenPlay) noexcept
{
    auto& l = loops[(size_t) i];
    const auto written = l.written.load();
    const auto master = i == 0 ? 0 : getMasterLength();

    if (master > 0)
    {
        const auto target = syncedLength (written, master, maxLength);
        if (written < target)
        {
            // Keep recording up to the synced length; process() closes it.
            l.recordTarget = target;
            l.stopAfterRecording = ! thenPlay;
            return;
        }

        // Went past it: drop the rest, and play on from where that puts us.
        closeRecording (i, target);
        if (l.length > 0)
        {
            if (thenPlay)
                playFrom (i, (written - target) % target);
            else
                l.state = State::stopped;
        }
        return;
    }

    closeRecording (i, written);
    if (l.length > 0)
    {
        if (thenPlay)
            startPlaying (i);
        else
            l.state = State::stopped;
    }
}

void LoopEngine::playFrom (int i, int offset) noexcept
{
    startPlaying (i);
    auto& l = loops[(size_t) i];
    if (l.state == State::playing)
        l.position = reverse ? (double) (l.length - 1 - offset) : (double) offset;
}

void LoopEngine::closeRecording (int i, int newLength) noexcept
{
    auto& l = loops[(size_t) i];
    l.recordTarget = -1;

    if (newLength < 2 * fadeLength)
    {
        // Too short to be a loop.
        l.state = State::empty;
        l.length = l.written = 0;
        return;
    }

    // Fades at both ends: the seam never clicks.
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        auto* data = l.audio.getWritePointer (ch);
        for (int k = 0; k < fadeLength; ++k)
        {
            const auto g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) k / (float) fadeLength);
            data[k] *= g;
            data[newLength - 1 - k] *= g;
        }
    }

    l.length = newLength;
    l.undoCount = 0;
    l.undone = false;

    // Loop 1 is the sync reference: its end is the downbeat.
    if (i == 0)
        clock = 0.0;
}

void LoopEngine::startPlaying (int i) noexcept
{
    auto& l = loops[(size_t) i];
    if (l.length <= 0)
    {
        l.state = State::empty;
        return;
    }

    if (l.role == Role::part)
    {
        stopOtherParts (i);
        currentPart = i;
    }

    l.position = reverse ? (double) (l.length - 1) : 0.0;
    l.state = State::playing;
    l.afterFade = AfterFade::nothing;
    l.queued = l.queuedRecord = false;
    l.gain.setTargetValue (1.0f);
}

void LoopEngine::requestRecording (int i) noexcept
{
    auto& l = loops[(size_t) i];
    if (i != 0 && startsOnDownbeat (i))
    {
        l.queued = true;
        l.queuedRecord = true;
    }
    else
    {
        beginRecording (i);
    }
}

void LoopEngine::requestPlaying (int i) noexcept
{
    auto& l = loops[(size_t) i];
    if (l.role == Role::part && currentPart >= 0 && currentPart != i && isRunning (currentPart))
    {
        // Another part plays: switch at its end.
        if (pendingPart >= 0)
            loops[(size_t) pendingPart].queued = false;
        pendingPart = i;
        l.queued = true;
    }
    else if (startsOnDownbeat (i))
    {
        l.queued = true;
        l.queuedRecord = false;
    }
    else
    {
        startPlaying (i);
    }
}

void LoopEngine::startOverdub (int i) noexcept
{
    // A new session: the previous undo (or redo) is gone.
    auto& l = loops[(size_t) i];
    l.undoStart = (int) l.position;
    l.sessionSaved = 0;
    l.undoCount = 0;
    l.undone = false;
    l.state = State::overdubbing;
}

void LoopEngine::fadeOut (int i, AfterFade after) noexcept
{
    auto& l = loops[(size_t) i];
    if (l.state == State::overdubbing)
        l.state = State::playing;          // stop writing at once, fade the sound out
    l.gain.setTargetValue (0.0f);
    l.afterFade = after;
    l.queued = false;
}

void LoopEngine::stopOtherParts (int except) noexcept
{
    for (int j = 0; j < kLoops; ++j)
    {
        if (j == except || loops[(size_t) j].role != Role::part)
            continue;

        auto& other = loops[(size_t) j];
        if (other.state == State::recording)
        {
            closeRecording (j, other.written);
            if (other.length > 0)
                other.state = State::stopped;
        }
        else if (other.state == State::playing || other.state == State::overdubbing)
        {
            fadeOut (j, AfterFade::stop);
        }
    }
}

void LoopEngine::continueSwap (int i) noexcept
{
    // Undo and redo are the same swap of the session's samples.
    auto& l = loops[(size_t) i];
    const auto count = l.undoCount.load();
    const auto length = l.length.load();
    const auto end = juce::jmin (count, l.swapCursor + kSwapPerSample);
    for (int k = l.swapCursor; k < end; ++k)
    {
        const auto idx = ((l.undoStart + k) % length + length) % length;
        for (int ch = 0; ch < kMaxChannels; ++ch)
            std::swap (l.audio.getWritePointer (ch)[idx], l.undo.getWritePointer (ch)[idx]);
    }

    l.swapCursor = end;
    if (l.swapCursor >= count)
    {
        l.swapCursor = -1;
        l.undone = ! l.undone.load();
    }
}

float LoopEngine::read (const Loop& l, int channel, double pos) const noexcept
{
    const auto* data = l.audio.getReadPointer (channel);
    const auto i0 = (int) pos;
    const auto frac = (float) (pos - i0);
    if (frac <= 0.0f)
        return data[i0];
    const auto i1 = i0 + 1 >= l.length ? 0 : i0 + 1;
    return data[i0] + frac * (data[i1] - data[i0]);
}

//==============================================================================
void LoopEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    if (maxLength == 0 || numChannels <= 0)
        return;

    const auto step = halfSpeed ? 0.5 : 1.0;

    for (int s = 0; s < numSamples; ++s)
    {
        for (int i = 0; i < kLoops; ++i)
            if (loops[(size_t) i].swapCursor >= 0)
                continueSwap (i);

        // The sync clock: queued loops start on its downbeat.
        const auto master = getMasterLength();
        auto downbeat = false;
        if (clockRunning && master > 0)
        {
            clock += step;
            if (clock >= master)
            {
                clock -= master;
                downbeat = true;
            }
        }
        if (downbeat)
            for (int i = 0; i < kLoops; ++i)
            {
                auto& l = loops[(size_t) i];
                if (! l.queued || i == pendingPart)
                    continue;
                if (l.queuedRecord && l.state == State::empty)
                    beginRecording (i);
                else if (! l.queuedRecord && l.state == State::stopped)
                    startPlaying (i);
                l.queued = l.queuedRecord = false;
            }

        // The loop is stereo; a mono input goes to both sides.
        float in[kMaxChannels], out[kMaxChannels];
        for (int ch = 0; ch < kMaxChannels; ++ch)
            out[ch] = in[ch] = io[juce::jmin (ch, numChannels - 1)][s];
        const auto loopLevel = level.getNextValue();

        for (int i = 0; i < kLoops; ++i)
        {
            auto& l = loops[(size_t) i];
            const auto gain = l.gain.getNextValue() * loopLevel;
            const auto state = l.state.load();

            if (state == State::recording)
            {
                const auto w = l.written.load();
                for (int ch = 0; ch < kMaxChannels; ++ch)
                    l.audio.getWritePointer (ch)[w] = in[ch];
                l.written = w + 1;

                // A synced recording closes at its length; any reaches the maximum.
                if ((l.recordTarget > 0 && w + 1 >= l.recordTarget) || w + 1 >= maxLength)
                {
                    if (l.recordTarget > 0)
                    {
                        const auto stopAfter = l.stopAfterRecording;
                        l.stopAfterRecording = false;
                        closeRecording (i, l.recordTarget);
                        if (l.length > 0)
                        {
                            if (stopAfter)
                                l.state = State::stopped;
                            else
                                startPlaying (i);
                        }
                    }
                    else
                    {
                        endRecording (i, true);
                    }
                }
                continue;
            }

            if ((state == State::playing || state == State::overdubbing) && l.length > 0)
            {
                const auto len = l.length.load();
                const auto idx = (int) l.position;
                for (int ch = 0; ch < kMaxChannels; ++ch)
                    out[ch] += gain * read (l, ch, l.position);

                if (state == State::overdubbing && ! halfSpeed)
                {
                    for (int ch = 0; ch < kMaxChannels; ++ch)
                    {
                        // Save each sample once per session, then add the input.
                        auto* data = l.audio.getWritePointer (ch);
                        if (l.sessionSaved < len)
                            l.undo.getWritePointer (ch)[idx] = data[idx];
                        data[idx] += in[ch];
                    }
                    if (l.sessionSaved < len)
                    {
                        ++l.sessionSaved;
                        l.undoCount = l.sessionSaved;
                        if (reverse)
                            l.undoStart = idx;     // a backward session's range starts at its lowest sample
                    }
                }

                l.position += reverse ? -step : step;
                auto wrapped = false;
                if (l.position >= len)
                {
                    l.position -= len;
                    wrapped = true;
                }
                if (l.position < 0.0)
                {
                    l.position += len;
                    wrapped = true;
                }

                // The playing part's end: a queued part takes over.
                if (wrapped && i == currentPart && pendingPart >= 0 && pendingPart != i)
                {
                    const auto next = pendingPart;
                    pendingPart = -1;
                    startPlaying (next);
                }
            }

            // A fade-out has finished: stop, or clear.
            if (l.afterFade != AfterFade::nothing && ! l.gain.isSmoothing() && l.gain.getCurrentValue() <= 0.0f)
            {
                if (l.afterFade == AfterFade::clear)
                {
                    l.state = State::empty;
                    l.length = l.written = 0;
                    l.undoCount = 0;
                    l.undone = false;
                }
                else
                {
                    l.state = State::stopped;
                }
                if (currentPart == i)
                    currentPart = -1;
                l.afterFade = AfterFade::nothing;
                l.position = 0.0;
            }
        }

        for (int ch = 0; ch < numChannels; ++ch)
            io[ch][s] = out[ch];

        // The clock runs while anything does, from the moment something starts.
        if (anyRunning())
        {
            if (! clockRunning)
            {
                clockRunning = true;
                clock = 0.0;
            }
        }
        else
        {
            clockRunning = false;
            clock = 0.0;
        }
    }

    for (auto& l : loops)
        l.shownPosition = l.position;
}
