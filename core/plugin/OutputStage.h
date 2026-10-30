#pragma once

#include "safety/Smoothing.h"

namespace rkfx
{

/** The standard controls every plugin ends with: dry/wet, output gain and
    bypass, in that order.

        out = bypassFade(dry, gain * (dry * cos(mix) + wet * sin(mix)))

    - Dry/wet is an equal-power crossfade (only used by plugins that have one;
      the others leave mix at 100%).
    - Output gain glides like every other continuous parameter.
    - Bypass crossfades to the untouched dry signal over about 10 ms.

    Exact at the ends: 0% and 100% mix, 0 dB output and full bypass are plain
    copies, so a transparent plugin nulls exactly against its input.

    GUI-free and allocation-free after prepare(), so the tests can drive it. */
class OutputStage
{
public:
    void prepare (double sampleRate);

    /** Jumps every glide to its target. */
    void reset();

    void setMix (float mix01) noexcept;            // 0..1
    void setOutputDecibels (float db) noexcept;
    void setBypassed (bool shouldBeBypassed) noexcept;

    /** False once bypass has fully faded in: the plugin can skip its wet
        processing entirely. */
    bool wetNeeded() const noexcept;

    /** True while bypass is engaged or fading (the dry signal is heard). */
    bool isBypassActive() const noexcept { return bypass.isSmoothing() || bypass.getTargetValue() > 0.0f; }

    /** Mixes @p dry into @p io (which holds the wet signal on entry) in place. */
    void process (const float* const* dry, float* const* io, int numChannels, int numSamples) noexcept;

private:
    Smoothed mix, bypass;
    SmoothedGain gain;

    static void mixGains (float m, float& dryGain, float& wetGain) noexcept;
};

} // namespace rkfx
