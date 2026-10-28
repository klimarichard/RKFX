#include "PitchDetector.h"

#include <cmath>

namespace rkboard
{

void PitchDetector::prepare (double newSampleRate, int windowSize)
{
    sampleRate = newSampleRate;
    window = windowSize;
    difference.assign ((size_t) window / 2, 0.0f);
}

float PitchDetector::detect (const float* x)
{
    const auto half = window / 2;
    const auto minLag = juce::jmax (2, (int) (sampleRate / 1500.0));
    const auto maxLag = juce::jmin (half - 1, (int) (sampleRate / 30.0));

    // Too quiet to tune
    double energy = 0.0;
    for (int i = 0; i < window; ++i)
        energy += x[i] * x[i];
    if (energy / window < 1.0e-7)
        return 0.0f;

    // Difference function, cumulative mean normalised (YIN steps 2 and 3)
    difference[0] = 1.0f;
    double running = 0.0;
    for (int lag = 1; lag < half; ++lag)
    {
        double sum = 0.0;
        for (int i = 0; i < half; ++i)
        {
            const auto d = x[i] - x[i + lag];
            sum += d * d;
        }
        running += sum;
        difference[(size_t) lag] = running > 0.0 ? (float) (sum * lag / running) : 1.0f;
    }

    // First dip under the threshold (step 4), then its local minimum
    constexpr float threshold = 0.12f;
    int best = -1;
    for (int lag = minLag; lag <= maxLag; ++lag)
        if (difference[(size_t) lag] < threshold)
        {
            while (lag + 1 <= maxLag && difference[(size_t) lag + 1] < difference[(size_t) lag])
                ++lag;
            best = lag;
            break;
        }

    if (best < 0)
        return 0.0f;

    // Parabolic interpolation around the minimum (step 5)
    auto lag = (float) best;
    if (best > 1 && best < half - 1)
    {
        const auto a = difference[(size_t) best - 1], b = difference[(size_t) best], c = difference[(size_t) best + 1];
        const auto denominator = a - 2.0f * b + c;
        if (std::abs (denominator) > 1.0e-9f)
            lag += 0.5f * (a - c) / denominator;
    }

    return (float) (sampleRate / lag);
}

PitchDetector::Note PitchDetector::noteFor (float hz, float reference)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const auto semitones = 12.0f * std::log2 (hz / reference) + 69.0f;   // MIDI note, fractional
    const auto nearest = juce::roundToInt (semitones);

    Note note;
    note.name = names[((nearest % 12) + 12) % 12];
    note.octave = nearest / 12 - 1;
    note.cents = 100.0f * (semitones - (float) nearest);
    return note;
}

} // namespace rkboard
