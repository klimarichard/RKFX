#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace rkboard
{

/** Pitch detection for the tuner (YIN, with parabolic interpolation).
    Runs on the message thread on a copy of the input, never on the audio. */
class PitchDetector
{
public:
    void prepare (double sampleRate, int windowSize = 4096);

    /** The fundamental of @p samples (windowSize of them) in Hz, or 0 when
        there is no clear pitch (silence, noise). Detects 30 Hz to 1.5 kHz. */
    float detect (const float* samples);

    int getWindowSize() const noexcept { return window; }

    struct Note
    {
        juce::String name;   // "A", "C#"
        int octave = 4;      // A4 = 440 Hz
        float cents = 0.0f;  // -50..+50 from the nearest note
    };

    /** The nearest equal-tempered note for @p hz, with A4 at @p reference Hz. */
    static Note noteFor (float hz, float reference = 440.0f);

private:
    double sampleRate = 48000.0;
    int window = 4096;
    std::vector<float> difference;
};

} // namespace rkboard
