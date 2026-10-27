#pragma once

#include <algorithm>
#include <cmath>

namespace rkfx
{

/** Zero-delay-feedback state-variable filter (TPT topology, Zavalishin),
    12 dB/oct. Stable and smooth under fast modulation; one instance per
    channel. setCutoff() costs one tan(), so call it per sub-block or on
    change, not per sample. */
class StateVariableFilter
{
public:
    enum class Type { lowpass, highpass, bandpass };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        setCutoff (cutoffHz, q);
        reset();
    }

    void reset() noexcept { s1 = s2 = bp = 0.0f; }

    /** @param resonanceQ  0.7071 = Butterworth. Cutoff is clamped below Nyquist. */
    void setCutoff (float hz, float resonanceQ = 0.70710678f) noexcept
    {
        cutoffHz = hz;
        q = resonanceQ;
        const auto clamped = std::clamp ((double) hz, 1.0, 0.49 * sampleRate);
        g = (float) std::tan (3.14159265358979323846 * clamped / sampleRate);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
    }

    /** The "g" coefficient for a cutoff (shared by every stage at that cutoff). */
    static float gFor (double hz, double rate) noexcept
    {
        return (float) std::tan (3.14159265358979323846 * std::clamp (hz, 1.0, 0.49 * rate) / rate);
    }

    /** Sets the coefficients directly, skipping tan(): for per-sample
        modulation where several stages share one g. k = 1 / Q. */
    void setCoefficients (float newG, float newK) noexcept
    {
        g = newG;
        k = newK;
        a1 = 1.0f / (1.0f + g * (g + k));
    }

    /** Band-pass scaled to 0 dB at the centre frequency, whatever the Q. */
    float processNormalisedBandpass (float x) noexcept
    {
        return k * process (x, Type::bandpass);
    }

    /** The band-pass output of the last process() call (for filters that
        need more than one output of the same update). */
    float lastBandpass() const noexcept { return bp; }

    float process (float x, Type type) noexcept
    {
        const auto hp = (x - (k + g) * s1 - s2) * a1;
        bp = g * hp + s1;
        const auto lp = g * bp + s2;
        s1 = g * hp + bp;
        s2 = g * bp + lp;

        switch (type)
        {
            case Type::highpass: return hp;
            case Type::bandpass: return bp;
            case Type::lowpass:  break;
        }
        return lp;
    }

private:
    double sampleRate = 48000.0;
    float cutoffHz = 1000.0f, q = 0.70710678f;
    float g = 0.0f, k = 1.4142f, a1 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f, bp = 0.0f;
};

} // namespace rkfx
