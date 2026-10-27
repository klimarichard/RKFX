#pragma once

#include "plugin/EditorBase.h"

class RKFreqShiftProcessor;

class RKFreqShiftEditor : public rkfx::EditorBase
{
public:
    explicit RKFreqShiftEditor (RKFreqShiftProcessor&);

private:
    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKFreqShiftEditor)
};
