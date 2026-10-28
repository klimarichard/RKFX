#include "LoopEngine.h"

#include <cmath>

void LoopEngine::prepare (double newSampleRate)
{
    fadeLength = juce::jmax (1, (int) (kFadeMs * 0.001 * newSampleRate));
    playGain.reset (newSampleRate, kFadeMs * 0.001);
    level.reset (newSampleRate, rkfx::kParamSmoothingSeconds);

    // Same rate: the loop stays (hosts prepare again for a new block size).
    if (juce::exactlyEqual (newSampleRate, sampleRate) && maxLength > 0)
        return;

    sampleRate = newSampleRate;
    maxLength = (int) (kMaxSeconds * sampleRate);
    loop.setSize (kMaxChannels, maxLength, false, false, false);
    undoBuffer.setSize (kMaxChannels, maxLength, false, false, false);

    state = State::empty;
    length = writeIndex = 0;
    undoCount = 0;
    undone = false;
    swapCursor = -1;
    position = 0.0;
    displayPosition = 0.0;
    playGain.setCurrentAndTargetValue (0.0f);
}

void LoopEngine::reset()
{
    if (state == State::playing || state == State::overdubbing)
        state = State::stopped;
    if (state == State::recording)
    {
        closeRecording();
        if (state == State::recording)
            state = State::stopped;
    }
    playGain.setCurrentAndTargetValue (0.0f);
    afterFade = AfterFade::nothing;
    position = 0.0;
}

void LoopEngine::press (Button button) noexcept
{
    if (maxLength == 0)
        return;

    switch (button)
    {
        case Button::record:
            switch (state.load())
            {
                case State::empty:       writeIndex = 0; state = State::recording; break;
                case State::recording:   closeRecording(); startPlaying (true); break;
                case State::playing:     if (! halfSpeed && swapCursor < 0) startOverdub(); break;
                case State::overdubbing: state = State::playing; break;
                case State::stopped:     startPlaying (true); break;
            }
            break;

        case Button::stop:
            if (state == State::recording)
            {
                closeRecording();
                state = State::stopped;
            }
            else if (state == State::playing || state == State::overdubbing)
            {
                state = State::playing;           // stop writing at once, fade the sound out
                playGain.setTargetValue (0.0f);
                afterFade = AfterFade::stop;
            }
            break;

        case Button::undo:
            if (state == State::overdubbing)
                state = State::playing;
            if (undoCount > 0 && swapCursor < 0 && (state == State::playing || state == State::stopped))
                swapCursor = 0;
            break;

        case Button::clear:
            if (state == State::recording)
            {
                state = State::empty;
                length = writeIndex = 0;
            }
            else if (state != State::empty)
            {
                if (state == State::overdubbing)
                    state = State::playing;
                playGain.setTargetValue (0.0f);
                afterFade = AfterFade::clear;
            }
            break;
    }
}

void LoopEngine::closeRecording() noexcept
{
    const auto n = writeIndex.load();
    if (n < 2 * fadeLength)
    {
        // Too short to be a loop.
        state = State::empty;
        length = writeIndex = 0;
        return;
    }

    // Fades at both ends: the seam never clicks.
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        auto* data = loop.getWritePointer (ch);
        for (int i = 0; i < fadeLength; ++i)
        {
            const auto g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) i / (float) fadeLength);
            data[i] *= g;
            data[n - 1 - i] *= g;
        }
    }

    length = n;
    undoCount = 0;
    undone = false;
}

void LoopEngine::startPlaying (bool fromStart) noexcept
{
    if (length <= 0)
    {
        state = State::empty;
        return;
    }

    if (fromStart)
        position = reverse ? (double) (length - 1) : 0.0;
    state = State::playing;
    afterFade = AfterFade::nothing;
    playGain.setTargetValue (1.0f);
}

void LoopEngine::startOverdub() noexcept
{
    // A new session: the previous undo (or redo) is gone.
    undoStart = (int) position;
    sessionSaved = 0;
    undoCount = 0;
    undone = false;
    state = State::overdubbing;
}

void LoopEngine::continueSwap() noexcept
{
    // Undo and redo are the same swap of the session's samples.
    const auto count = undoCount.load();
    const auto end = juce::jmin (count, swapCursor + kSwapPerSample);
    for (int k = swapCursor; k < end; ++k)
    {
        const auto i = wrap (undoStart + k);
        for (int ch = 0; ch < kMaxChannels; ++ch)
            std::swap (loop.getWritePointer (ch)[i], undoBuffer.getWritePointer (ch)[i]);
    }

    swapCursor = end;
    if (swapCursor >= count)
    {
        swapCursor = -1;
        undone = ! undone.load();
    }
}

float LoopEngine::read (int channel, double pos) const noexcept
{
    const auto* data = loop.getReadPointer (channel);
    const auto i0 = (int) pos;
    const auto frac = (float) (pos - i0);
    if (frac <= 0.0f)
        return data[i0];
    const auto i1 = i0 + 1 >= length ? 0 : i0 + 1;
    return data[i0] + frac * (data[i1] - data[i0]);
}

void LoopEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = juce::jmin (numChannels, kMaxChannels);
    if (maxLength == 0 || numChannels <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        if (swapCursor >= 0)
            continueSwap();

        const auto s = state.load();
        const auto gain = playGain.getNextValue() * level.getNextValue();

        if (s == State::recording)
        {
            const auto w = writeIndex.load();
            for (int ch = 0; ch < kMaxChannels; ++ch)
                loop.getWritePointer (ch)[w] = io[juce::jmin (ch, numChannels - 1)][i];
            writeIndex = w + 1;
            if (w + 1 >= maxLength)
            {
                closeRecording();
                startPlaying (true);
            }
            continue;
        }

        if ((s == State::playing || s == State::overdubbing) && length > 0)
        {
            const auto len = length.load();
            const auto dubbing = s == State::overdubbing && ! halfSpeed;   // half speed: playback only
            const auto idx = (int) position;

            // The loop is stereo; a mono input goes to both sides.
            float in[kMaxChannels];
            for (int ch = 0; ch < kMaxChannels; ++ch)
                in[ch] = io[juce::jmin (ch, numChannels - 1)][i];

            for (int ch = 0; ch < numChannels; ++ch)
                io[ch][i] = in[ch] + gain * read (ch, position);

            if (dubbing)
                for (int ch = 0; ch < kMaxChannels; ++ch)
                {
                    // Save each sample once per session, then add the input.
                    auto* data = loop.getWritePointer (ch);
                    if (sessionSaved < len)
                        undoBuffer.getWritePointer (ch)[idx] = data[idx];
                    data[idx] += in[ch];
                }

            if (dubbing && sessionSaved < len)
            {
                ++sessionSaved;
                undoCount = sessionSaved;
                if (reverse)
                    undoStart = (int) position;   // a backward session's range starts at its lowest sample
            }

            const auto step = halfSpeed ? 0.5 : 1.0;
            position += reverse ? -step : step;
            if (position >= len)
                position -= len;
            if (position < 0.0)
                position += len;
        }

        // A fade-out has finished: stop, or clear.
        if (afterFade != AfterFade::nothing && ! playGain.isSmoothing() && playGain.getCurrentValue() <= 0.0f)
        {
            if (afterFade == AfterFade::clear)
            {
                state = State::empty;
                length = writeIndex = 0;
                undoCount = 0;
                undone = false;
            }
            else
            {
                state = State::stopped;
            }
            afterFade = AfterFade::nothing;
            position = 0.0;
        }
    }

    displayPosition = position;
}
