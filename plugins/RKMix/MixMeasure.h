#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

/** Monitor EQ's measuring (docs/design/RKMix Monitor EQ.md): pink noise out
    of one Aux bus, and the noise and one input, each split into RKGraphEQ's
    31 1/3-octave bands, summed as power.

    The engine asks process() once per block. Idle, that is one atomic read
    and nothing else: no noise, no filters, no sums. While measuring:

        start()  the bus is taken over: its own signal and its fader fade to
                 the noise path over 50 ms (takeover() goes 0 -> 1)
        setNoise (true, dB)  the noise fades in over 1 s (out over 50 ms)
        stop()   the noise fades out, then the bus fades back over 50 ms;
                 isBusy() turns false when the bus is the bus's again

    The sums run from start() and are published after every block under a
    sequence counter; read() returns a consistent copy. The message thread
    windows them (the difference between two reads) and does the maths.

    Threads: start(), setNoise(), stop(), read(), isBusy() on the message
    thread; prepare() with the audio stopped; process() on the audio thread.
    Allocation-free and lock-free after prepare(). GUI-free. */
class MixMeasure
{
    /** An atomic that copies its value (the engine is copied in tests, between runs). */
    template <typename T>
    struct Shared : std::atomic<T>
    {
        Shared() noexcept : std::atomic<T> (T {}) {}
        Shared (T v) noexcept : std::atomic<T> (v) {}
        Shared (const Shared& o) noexcept : std::atomic<T> (o.load()) {}
        Shared& operator= (const Shared& o) noexcept { this->store (o.load()); return *this; }
    };

public:
    static constexpr int kBands = 31;
    static constexpr float kMinNoiseDb = -60.0f, kMaxNoiseDb = -12.0f;   // RMS, dBFS
    static constexpr double kTakeoverSeconds = 0.05, kNoiseUpSeconds = 1.0, kNoiseDownSeconds = 0.05;

    /** Which sides of a stereo bus carry the noise (the other is silent). */
    enum class Sides { both, left, right };

    /** RKGraphEQ's 31 centres, 20 Hz .. 20 kHz. */
    static const std::array<double, kBands>& centres() noexcept;

    /** The pair of band-pass sections' Q: -3 dB at the band edges (1/6 octave each side). */
    static double bandQ() noexcept;

    /** True when band @p band can be measured at @p sampleRate (its centre below 0.45 x the rate). */
    static bool isMeasured (int band, double sampleRate) noexcept;

    /** The running sums since start(). */
    struct Sums
    {
        std::int64_t samples = 0;                    // since start()
        std::int64_t steadySamples = 0;              // of those, with the noise steady at its level (or off and steady)
        std::array<double, kBands> reference {};     // sum of squares of the noise, per band
        std::array<double, kBands> input {};         // of the input, per band
        float inputPeak = 0.0f;                      // the input's loudest sample since start()
        bool steady = false;                         // the bus taken over and the noise at its target now
        bool noiseOn = false;                        // the target, as the audio thread last saw it
    };

    //==============================================================================
    void prepare (double sampleRate, int maxBlockSize);

    //==============================================================================
    // Message thread

    /** Takes over Aux bus @p bus, listening to input @p input (0-based, before trim).
        Returns false (and does nothing) while a run is still busy. */
    bool start (int bus, int input, Sides sides) noexcept;
    void setNoise (bool on, float levelDb) noexcept;
    void stop() noexcept;

    /** From start() until the bus has been handed back after stop(). */
    bool isBusy() const noexcept { return requested.load (std::memory_order_acquire) || active.load (std::memory_order_acquire); }

    /** A consistent copy of the sums (all zero before the first block of a run). */
    Sums read() const noexcept;

    double getSampleRate() const noexcept { return rate; }

    //==============================================================================
    // Audio thread

    /** One block: the bus being taken over (or -1, after which nothing
        else here needs calling). @p in are the engine's 32 inputs. */
    int process (const float* const* in, int numInputs, int numSamples) noexcept;

    /** This block's noise (already at its level and takeover), and the takeover (0..1), per sample. */
    const float* noise() const noexcept { return noiseBuffer.data(); }
    const float* takeover() const noexcept { return takeoverBuffer.data(); }
    bool noiseOnSide (int side, int numSides) const noexcept;

private:
    /** A 2nd-order band-pass in double precision (the low bands are far below the rate). */
    struct Section
    {
        double b0 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;   // b1 = 0
        double z1 = 0.0, z2 = 0.0;
        double run (double x) noexcept
        {
            const auto y = b0 * x + z1;
            z1 = -a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() noexcept { z1 = z2 = 0.0; }
    };

    struct Band
    {
        std::array<Section, 2> reference, input;
        void clear() noexcept;
    };

    float nextPink() noexcept;
    void publish() noexcept;
    void beginRun() noexcept;

    double rate = 48000.0;
    int maxBlock = 0;
    int numMeasured = kBands;                        // the bands below 0.45 x the rate

    // Requests (message thread -> audio thread)
    Shared<bool> requested { false };
    Shared<int> requestBus { -1 }, requestInput { 0 }, requestSides { 0 };
    Shared<bool> noiseRequested { false };
    Shared<float> noiseGainRequested { 0.0f };

    // The audio thread's run
    Shared<bool> active { false };              // the bus is (still) taken over
    int bus = -1, input = 0;
    Sides sides = Sides::both;
    bool stopping = false;
    float takeoverGain = 0.0f, noiseGain = 0.0f;
    std::array<Band, kBands> bands;
    std::array<double, kBands> referenceSums {}, inputSums {};
    std::int64_t samples = 0, steadySamples = 0;
    float inputPeak = 0.0f;
    bool steadyNow = false, noiseOnNow = false;

    // Pink noise (Paul Kellet's refined filter on white noise)
    std::uint32_t seed = 1;
    std::array<float, 7> pink {};
    float pinkScale = 1.0f;

    std::vector<float> noiseBuffer, takeoverBuffer;

    // The published sums (a seqlock: odd while being written)
    Shared<std::uint32_t> sequence { 0 };
    std::array<Shared<double>, kBands> sharedReference {}, sharedInput {};
    Shared<std::int64_t> sharedSamples { 0 }, sharedSteadySamples { 0 };
    Shared<float> sharedPeak { 0.0f };
    Shared<bool> sharedSteady { false }, sharedNoiseOn { false };
};
