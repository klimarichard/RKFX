#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "BandDisplay.h"
#include "MultiCompEngine.h"

#include <array>

class RKMultiCompProcessor;

class RKMultiCompEditor : public rkfx::EditorBase
{
public:
    explicit RKMultiCompEditor (RKMultiCompProcessor&);

private:
    using Band = MultiCompEngine::Band;

    /** One tab per band in use: name and gain reduction; a click selects the
        band. Its On and Solo switches sit on top of it. */
    class BandTabs : public juce::Component
    {
    public:
        BandTabs (RKMultiCompEditor& o) : owner (o) {}
        void setReduction (Band, float reductionDb, float secondsSinceLast);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        RKMultiCompEditor& owner;
        std::array<float, MultiCompEngine::kNumBands> shown {};
    };

    struct BandControls
    {
        std::unique_ptr<rkfx::Knob> threshold, ratio, attack, release, gain;
        std::unique_ptr<rkfx::ToggleSwitch> on, solo;
    };

    RKMultiCompProcessor& comp;
    MultiCompEngine::Settings settings;
    std::array<Band, MultiCompEngine::kNumBands> bandList {};
    int numBands = 3;

    BandDisplay display;
    BandTabs tabs { *this };
    rkfx::ParameterWatcher watcher;

    std::array<BandControls, MultiCompEngine::kNumBands> bandControls;
    std::unique_ptr<rkfx::Knob> xoverLow, xoverMid, xoverHigh, offset, knee, link;
    std::unique_ptr<rkfx::SegmentedSwitch> bands, detect;
    std::unique_ptr<rkfx::ToggleSwitch> releaseAuto, gainAuto;

    double lastMeterTime = 0.0;
    juce::TimedCallback meterTimer { [this] { updateMeters(); } };

    Band selectedBand() const;
    void selectBand (Band);
    juce::Rectangle<int> tabArea (int index) const;

    juce::Rectangle<int> displayArea() const;
    juce::Rectangle<int> tabsArea() const;
    juce::Rectangle<int> bandSection() const;
    juce::Rectangle<int> crossoverSection() const;

    void onParameterChange();
    void updateMeters();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKMultiCompEditor)
};
