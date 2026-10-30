#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/** The volume pedal's gain: position through a taper, never below the
    minimum level, smoothed (20 ms) so fast pedal moves never zipper. */
class VolumeEngine
{
public:
    static constexpr float kSilentDb = -100.0f;   // "-inf"

    struct Settings
    {
        float position = 1.0f;      // 0..1 (heel..toe)
        bool audioTaper = true;     // false = linear
        float minLevelDb = kSilentDb;
    };

    /** Linear: gain follows the position. Audio: position cubed, about -18 dB
        at the middle, like a log pot, so the pedal swells evenly to the ear. */
    static float gainFor (const Settings& s)
    {
        const auto p = juce::jlimit (0.0f, 1.0f, s.position);
        const auto t = s.audioTaper ? p * p * p : p;
        const auto minimum = s.minLevelDb <= -80.0f ? 0.0f : juce::Decibels::decibelsToGain (s.minLevelDb);
        return minimum + (1.0f - minimum) * t;
    }

    void prepare (double sampleRate) { gain.reset (sampleRate, 0.02); }
    void reset (const Settings& s) { gain.setCurrentAndTargetValue (gainFor (s)); }

    void process (juce::AudioBuffer<float>& buffer, const Settings& s)
    {
        gain.setTargetValue (gainFor (s));
        gain.applyGain (buffer, buffer.getNumSamples());
    }

private:
    juce::SmoothedValue<float> gain { 1.0f };
};
