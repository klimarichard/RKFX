#include "MixMeasure.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    float gainOf (float db) noexcept { return std::pow (10.0f, db / 20.0f); }

    inline bool same (float a, float b) noexcept { return ! (a < b) && ! (b < a); }

    /** Moves @p value toward @p target by @p step, landing on it exactly. */
    inline void moveToward (float& value, float target, float step) noexcept
    {
        value = value < target ? std::min (target, value + step) : std::max (target, value - step);
    }
}

const std::array<double, MixMeasure::kBands>& MixMeasure::centres() noexcept
{
    static constexpr std::array<double, kBands> c { 20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 200.0,
                                                    250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0, 2000.0,
                                                    2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0,
                                                    20000.0 };
    return c;
}

double MixMeasure::bandQ() noexcept
{
    // Each section's |H|^2 = 1 / (1 + Q^2 x^2), x = f/f0 - f0/f. Two in series are
    // -3 dB where one is 1/sqrt 2: at the band edges, f0 x 2^(+-1/6).
    const auto x = std::pow (2.0, 1.0 / 6.0) - std::pow (2.0, -1.0 / 6.0);
    return std::sqrt (std::sqrt (2.0) - 1.0) / x;
}

bool MixMeasure::isMeasured (int band, double sampleRate) noexcept
{
    return band >= 0 && band < kBands && centres()[(size_t) band] < 0.45 * sampleRate;
}

void MixMeasure::Band::clear() noexcept
{
    for (auto* sections : { &reference, &input })
        for (auto& s : *sections)
            s.clear();
}

//==============================================================================
void MixMeasure::prepare (double sampleRate, int maxBlockSize)
{
    rate = sampleRate;
    maxBlock = std::max (1, maxBlockSize);
    noiseBuffer.assign ((size_t) maxBlock, 0.0f);
    takeoverBuffer.assign ((size_t) maxBlock, 0.0f);

    numMeasured = 0;
    while (numMeasured < kBands && isMeasured (numMeasured, rate))
        ++numMeasured;

    // RBJ band-pass (0 dB at the centre), the same for both sections.
    const auto q = bandQ();
    for (int b = 0; b < kBands; ++b)
    {
        const auto w = 2.0 * kPi * std::min (centres()[(size_t) b], 0.45 * rate) / rate;
        const auto alpha = std::sin (w) / (2.0 * q);
        const auto a0 = 1.0 + alpha;
        Section s;
        s.b0 = alpha / a0;
        s.b2 = -alpha / a0;
        s.a1 = -2.0 * std::cos (w) / a0;
        s.a2 = (1.0 - alpha) / a0;
        auto& band = bands[(size_t) b];
        band.reference.fill (s);
        band.input.fill (s);
    }

    // Pink noise at an RMS of 1 (0 dBFS RMS before its level).
    seed = 1;
    pink = {};
    pinkScale = 1.0f;
    double sum = 0.0;
    constexpr int n = 1 << 16;
    for (int i = 0; i < n; ++i)
    {
        const auto x = nextPink();
        sum += (double) x * x;
    }
    pinkScale = (float) (1.0 / std::sqrt (sum / n));

    // Prepared with the audio stopped: any run is over.
    requested.store (false);
    active.store (false);
    bus = -1;
    stopping = false;
    takeoverGain = noiseGain = 0.0f;
}

//==============================================================================
bool MixMeasure::start (int busIndex, int inputIndex, Sides s) noexcept
{
    if (isBusy())
        return false;
    requestBus.store (busIndex, std::memory_order_relaxed);
    requestInput.store (inputIndex, std::memory_order_relaxed);
    requestSides.store ((int) s, std::memory_order_relaxed);
    noiseRequested.store (false, std::memory_order_relaxed);
    requested.store (true, std::memory_order_release);
    return true;
}

void MixMeasure::setNoise (bool on, float levelDb) noexcept
{
    noiseGainRequested.store (gainOf (std::clamp (levelDb, kMinNoiseDb, kMaxNoiseDb)), std::memory_order_relaxed);
    noiseRequested.store (on, std::memory_order_release);
}

void MixMeasure::stop() noexcept
{
    noiseRequested.store (false, std::memory_order_relaxed);
    requested.store (false, std::memory_order_release);
}

MixMeasure::Sums MixMeasure::read() const noexcept
{
    Sums s;
    for (;;)
    {
        const auto before = sequence.load (std::memory_order_acquire);
        if ((before & 1u) != 0)
            continue;
        for (int b = 0; b < kBands; ++b)
        {
            s.reference[(size_t) b] = sharedReference[(size_t) b].load (std::memory_order_relaxed);
            s.input[(size_t) b] = sharedInput[(size_t) b].load (std::memory_order_relaxed);
        }
        s.samples = sharedSamples.load (std::memory_order_relaxed);
        s.steadySamples = sharedSteadySamples.load (std::memory_order_relaxed);
        s.inputPeak = sharedPeak.load (std::memory_order_relaxed);
        s.steady = sharedSteady.load (std::memory_order_relaxed);
        s.noiseOn = sharedNoiseOn.load (std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_acquire);
        if (sequence.load (std::memory_order_relaxed) == before)
            return s;
    }
}

//==============================================================================
float MixMeasure::nextPink() noexcept
{
    // xorshift32 white noise, -1 .. 1
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    const auto white = (float) ((double) seed / 2147483648.0 - 1.0);

    // Paul Kellet's refined pink filter: -3 dB/octave within 0.05 dB above 9 Hz.
    auto& b = pink;
    b[0] = 0.99886f * b[0] + white * 0.0555179f;
    b[1] = 0.99332f * b[1] + white * 0.0750759f;
    b[2] = 0.96900f * b[2] + white * 0.1538520f;
    b[3] = 0.86650f * b[3] + white * 0.3104856f;
    b[4] = 0.55000f * b[4] + white * 0.5329522f;
    b[5] = -0.7616f * b[5] - white * 0.0168980f;
    const auto out = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + white * 0.5362f;
    b[6] = white * 0.115926f;
    return out * pinkScale;
}

void MixMeasure::beginRun() noexcept
{
    bus = requestBus.load (std::memory_order_relaxed);
    input = requestInput.load (std::memory_order_relaxed);
    sides = (Sides) requestSides.load (std::memory_order_relaxed);
    stopping = false;
    takeoverGain = noiseGain = 0.0f;
    for (auto& b : bands)
        b.clear();
    referenceSums.fill (0.0);
    inputSums.fill (0.0);
    samples = steadySamples = 0;
    inputPeak = 0.0f;
    steadyNow = noiseOnNow = false;
    seed = 1;
    pink = {};
}

bool MixMeasure::noiseOnSide (int side, int numSides) const noexcept
{
    if (numSides < 2 || sides == Sides::both)
        return true;
    return (sides == Sides::left) == (side == 0);
}

int MixMeasure::process (const float* const* in, int numInputs, int numSamples) noexcept
{
    // Idle: this one read.
    const auto wanted = requested.load (std::memory_order_acquire);
    if (! active.load (std::memory_order_relaxed))
    {
        if (! wanted)
            return -1;
        beginRun();
        active.store (true, std::memory_order_release);
    }

    numSamples = std::min (numSamples, maxBlock);
    if (! wanted)
        stopping = true;
    const auto noiseOn = ! stopping && noiseRequested.load (std::memory_order_acquire);
    const auto noiseTarget = noiseOn ? noiseGainRequested.load (std::memory_order_relaxed) : 0.0f;

    // The takeover and the noise, sample by sample: the noise rises only once
    // the bus is taken over, and the bus is handed back only once it is silent.
    const auto takeoverStep = (float) (1.0 / (kTakeoverSeconds * rate));
    const auto steadyAtStart = steadyNow;
    for (int i = 0; i < numSamples; ++i)
    {
        const auto target = takeoverGain < 1.0f ? 0.0f : noiseTarget;
        if (! same (noiseGain, target))
        {
            const auto seconds = target > noiseGain ? kNoiseUpSeconds : kNoiseDownSeconds;
            // The ramp's full height over its time: up to the level, down from the loudest level.
            const auto height = target > noiseGain ? target : gainOf (kMaxNoiseDb);
            moveToward (noiseGain, target, (float) (height / (seconds * rate)));
        }
        if (stopping)
        {
            if (noiseGain <= 0.0f)
                takeoverGain = std::max (0.0f, takeoverGain - takeoverStep);
        }
        else
        {
            takeoverGain = std::min (1.0f, takeoverGain + takeoverStep);
        }

        takeoverBuffer[(size_t) i] = takeoverGain;
        noiseBuffer[(size_t) i] = noiseGain > 0.0f ? nextPink() * noiseGain : 0.0f;
    }
    steadyNow = ! stopping && takeoverGain >= 1.0f && same (noiseGain, noiseTarget);
    noiseOnNow = noiseOn;

    // The bands: the noise as sent, and the input as it arrives.
    const auto* mic = input >= 0 && input < numInputs ? in[input] : nullptr;
    for (int b = 0; b < numMeasured; ++b)
    {
        auto& band = bands[(size_t) b];
        double ref = 0.0, inp = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto r = band.reference[1].run (band.reference[0].run ((double) noiseBuffer[(size_t) i]));
            ref += r * r;
            if (mic != nullptr)
            {
                const auto m = band.input[1].run (band.input[0].run ((double) mic[i]));
                inp += m * m;
            }
        }
        referenceSums[(size_t) b] += ref;
        inputSums[(size_t) b] += inp;
    }
    if (mic != nullptr)
        for (int i = 0; i < numSamples; ++i)
            inputPeak = std::max (inputPeak, std::abs (mic[i]));

    samples += numSamples;
    if (steadyAtStart && steadyNow)
        steadySamples += numSamples;
    publish();

    const auto taken = bus;
    if (stopping && takeoverGain <= 0.0f)
    {
        bus = -1;
        active.store (false, std::memory_order_release);
    }
    return taken;
}

void MixMeasure::publish() noexcept
{
    const auto s = sequence.load (std::memory_order_relaxed);
    sequence.store (s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence (std::memory_order_release);
    for (int b = 0; b < kBands; ++b)
    {
        sharedReference[(size_t) b].store (referenceSums[(size_t) b], std::memory_order_relaxed);
        sharedInput[(size_t) b].store (inputSums[(size_t) b], std::memory_order_relaxed);
    }
    sharedSamples.store (samples, std::memory_order_relaxed);
    sharedSteadySamples.store (steadySamples, std::memory_order_relaxed);
    sharedPeak.store (inputPeak, std::memory_order_relaxed);
    sharedSteady.store (steadyNow, std::memory_order_relaxed);
    sharedNoiseOn.store (noiseOnNow, std::memory_order_relaxed);
    sequence.store (s + 2, std::memory_order_release);
}
