#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

/** The RKRemap DSP: a 64 x 64 patch bay.

    Each output takes a Source: off, one input, or a linked input pair
    (summed to mono on a single output, or left/right on a linked output
    pair, or mono there too). A linked output pair follows its left output's
    settings. Trim, mute and polarity glide (kFadeSeconds); a source change
    crossfades from the old route to the new one. A steady route at unity
    is a bit-exact copy. Allocation-free after prepare(). */
class RemapEngine
{
public:
    static constexpr int kChannels = 64;
    static constexpr int kPairs = kChannels / 2;
    static constexpr int kSourceOff = 0;
    static constexpr int kFirstPairSource = kChannels + 1;   // 65 = In 1/2 ... 96 = In 63/64
    static constexpr double kFadeSeconds = 0.01;

    struct Output
    {
        int source = 0;           // 0 off, 1..64 an input, 65..96 an input pair
        float trimDb = 0.0f;
        bool mute = false, polarity = false, mono = false;
    };

    struct Settings
    {
        std::array<Output, kChannels> outputs {};
        std::array<bool, kPairs> inputLinks {}, outputLinks {};
    };

    /** What output @p index plays: inputs a and b (b >= 0: their average), or a < 0 for silence. */
    struct Route
    {
        int a = -1, b = -1;
        bool operator== (const Route& o) const noexcept { return a == o.a && b == o.b; }
        bool operator!= (const Route& o) const noexcept { return ! (*this == o); }
    };
    static Route routeOf (const Settings&, int output);

    /** The output whose controls output @p index follows (its pair's left when linked). */
    static int leaderOf (const Settings&, int output);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setSettings (const Settings&);

    void process (float* const* io, int numChannels, int numSamples) noexcept;

    /** The loudest sample since the last call, per input and per output (resets). */
    float takeInputPeak (int channel) noexcept;
    float takeOutputPeak (int channel) noexcept;

private:
    struct OutputState
    {
        Route route, previous;
        int fadeLeft = 0;             // samples of the route crossfade still to go
        float gain = 1.0f, targetGain = 1.0f, gainStep = 0.0f;
    };

    double sampleRate = 48000.0;
    int fadeSamples = 480;
    Settings settings;
    std::array<OutputState, kChannels> states;
    juce::AudioBuffer<float> inputs;  // a copy: the outputs overwrite the same buffer
    std::array<float, kChannels> inputPeaks {}, outputPeaks {};

    float gainFor (int output) const noexcept;
};
