#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rkfx
{

/** Vertical gain reduction meter: a bar growing down from 0 dB, scale 0 to
    -24 dB. The editor feeds it from a ~30 fps timer; it falls back gently
    (about 20 dB/s) and repaints only when the shown value changes. */
class GainReductionMeter : public juce::Component
{
public:
    explicit GainReductionMeter (juce::Colour accent);

    /** @param reductionDb  0 or negative */
    void setReduction (float reductionDb, float secondsSinceLast);

    void paint (juce::Graphics&) override;

    static constexpr float kRangeDb = 24.0f;

private:
    juce::Colour accent;
    float shownDb = 0.0f;
};

} // namespace rkfx
