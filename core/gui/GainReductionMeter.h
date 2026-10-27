#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rkfx
{

/** Vertical gain reduction meter: a bar growing down from 0 dB, scale 0 to
    -24 dB (or another range). The editor feeds it from a ~30 fps timer; it falls back gently
    (about 20 dB/s) and repaints only when the shown value changes. */
class GainReductionMeter : public juce::Component
{
public:
    explicit GainReductionMeter (juce::Colour accent, float rangeDb = 24.0f);

    /** @param reductionDb  0 or negative */
    void setReduction (float reductionDb, float secondsSinceLast);

    void paint (juce::Graphics&) override;

private:
    juce::Colour accent;
    float rangeDb;
    float shownDb = 0.0f;
};

} // namespace rkfx
