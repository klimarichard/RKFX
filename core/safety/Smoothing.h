#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace rkfx
{

/** Suite-wide glide times. Every continuous parameter glides over
    kParamSmoothingSeconds (inside the 5 to 20 ms the spec allows), and bypass
    crossfades over kBypassFadeSeconds. */
inline constexpr double kParamSmoothingSeconds = 0.02;
inline constexpr double kBypassFadeSeconds     = 0.01;

/** Linear smoother used for every continuous parameter. */
using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

/** Multiplicative smoother, for gains (glides evenly in dB). Values must stay > 0. */
using SmoothedGain = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;

} // namespace rkfx
