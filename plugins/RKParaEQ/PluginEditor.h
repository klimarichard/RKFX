#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "Parameters.h"

class RKParaEQProcessor;

class RKParaEQEditor : public rkfx::EditorBase
{
public:
    explicit RKParaEQEditor (RKParaEQProcessor&);

private:
    /** One band's controls; knobs dim while the band is off, Q while it's a shelf. */
    struct Strip
    {
        Strip (rkfx::ProcessorBase&, const ParaEQParams::BandInfo&, juce::Colour accent);

        std::unique_ptr<rkfx::ToggleSwitch> on;
        std::unique_ptr<rkfx::SegmentedSwitch> type, slope;
        std::unique_ptr<rkfx::Knob> freq, gain, q;
        std::unique_ptr<juce::ParameterAttachment> onWatcher, typeWatcher;
        bool isOn = true, isShelf = false;

        void updateDimming();
    };

    RKParaEQProcessor& eq;
    std::vector<std::unique_ptr<Strip>> strips;
    rkfx::ResponseCurve curve;
    rkfx::ParameterWatcher watcher;

    juce::Rectangle<int> curveArea() const;
    juce::Rectangle<int> stripArea (int band) const;

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKParaEQEditor)
};
