#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <optional>

namespace rkboard
{

/** Tap tempo (standalone): the average of the last taps' intervals (up to
    four); a pause of more than two seconds starts over. No allocation, so it
    can run on the audio thread (a MIDI footswitch). */
class TapTempo
{
public:
    /** Returns the tempo once there are two taps or more. */
    std::optional<double> tap (double timeSeconds)
    {
        if (count > 0 && timeSeconds - times[(size_t) ((count - 1) % kMax)] > 2.0)
            count = 0;

        times[(size_t) (count % kMax)] = timeSeconds;
        ++count;
        if (count < 2)
            return std::nullopt;

        const auto taps = juce::jmin (count, kMax);
        const auto first = times[(size_t) ((count - taps) % kMax)];
        const auto bpm = 60.0 * (taps - 1) / (timeSeconds - first);
        return juce::jlimit (20.0, 300.0, bpm);
    }

private:
    static constexpr int kMax = 5;
    std::array<double, kMax> times {};
    int count = 0;
};

/** Follows incoming MIDI clock (24 ticks per quarter note): the tempo over the
    last 24 ticks. Audio thread. */
class MidiClockFollower
{
public:
    void reset() { count = 0; }

    /** A clock tick at @p timeSeconds. */
    void tick (double timeSeconds)
    {
        times[(size_t) (count % kTicks)] = timeSeconds;
        ++count;
    }

    /** The tempo, once a quarter note of clock has arrived (and while ticks keep coming). */
    std::optional<double> bpm (double now) const
    {
        if (count <= kTicks || now - times[(size_t) ((count - 1) % kTicks)] > 0.5)
            return std::nullopt;

        // The ring holds the last 24 ticks: 23 intervals of 1/24 quarter note each.
        const auto newest = times[(size_t) ((count - 1) % kTicks)];
        const auto oldest = times[(size_t) (count % kTicks)];
        const auto quarterNote = (newest - oldest) * kTicks / (kTicks - 1);
        return quarterNote > 0.0 ? std::optional<double> (juce::jlimit (20.0, 300.0, 60.0 / quarterNote))
                                 : std::nullopt;
    }

private:
    static constexpr int kTicks = 24;
    std::array<double, kTicks> times {};
    int count = 0;
};

/** The standalone app has no host: the board is the pedals' play head, with
    its own tempo and a running beat position (for synced delays and LFOs). */
class BoardPlayHead : public juce::AudioPlayHead
{
public:
    std::atomic<double> bpm { 120.0 };

    void advance (int numSamples, double sampleRate)
    {
        ppq = ppq.load() + numSamples * bpm.load() / (60.0 * sampleRate);
    }

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm.load());
        info.setPpqPosition (ppq.load());
        info.setIsPlaying (true);
        return info;
    }

private:
    std::atomic<double> ppq { 0.0 };
};

} // namespace rkboard
