#pragma once

#include "plugin/EditorBase.h"

#include "Parameters.h"

class RKFreqShiftProcessor;

class RKFreqShiftEditor : public rkfx::EditorBase
{
public:
    explicit RKFreqShiftEditor (RKFreqShiftProcessor&);

private:
    /** One shift's controls. The frequency knob shows the fine or coarse
        parameter depending on the range switch; knobs dim while the shift is off. */
    struct Strip
    {
        Strip (rkfx::ProcessorBase&, int shift, juce::Colour accent);

        std::unique_ptr<rkfx::ToggleSwitch> on;
        std::unique_ptr<rkfx::SegmentedSwitch> range;
        std::unique_ptr<rkfx::Knob> coarse, fine, updown, feedback, level;
        std::unique_ptr<juce::ParameterAttachment> rangeWatcher, onWatcher;
    };

    std::vector<std::unique_ptr<Strip>> strips;
    std::unique_ptr<rkfx::Knob> spread, lowCut, highCut;

    juce::Rectangle<int> stripArea (int shift) const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFreqShiftEditor)
};
