#pragma once

#include "plugin/EditorBase.h"
#include "gui/ResponseCurve.h"

#include "ParaEQEngine.h"
#include "Parameters.h"

#include <array>

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

        std::unique_ptr<rkfx::ToggleSwitch> on, dyn;
        std::unique_ptr<rkfx::SegmentedSwitch> type, slope;
        std::unique_ptr<rkfx::Knob> freq, gain, q;
        std::unique_ptr<juce::ParameterAttachment> onWatcher, typeWatcher;
        bool isOn = true, isShelf = false;

        void updateDimming();
    };

    /** A gain band's dynamics controls, shown in the dynamics row while the
        band is selected. */
    struct DynamicControls
    {
        std::unique_ptr<rkfx::ToggleSwitch> dynamic;
        std::unique_ptr<rkfx::Knob> threshold, range, ratio, attack, release;
    };

    /** Clicking a gain band's strip (anywhere but its controls) selects it. */
    struct StripSelector : public juce::Component
    {
        StripSelector (RKParaEQEditor& o, int b) : owner (o), band (b) {}
        void mouseDown (const juce::MouseEvent&) override { owner.selectBand (band); }
        RKParaEQEditor& owner;
        const int band;
    };

    /** Over the response curve: the EQ as it is right now (with the moving
        dynamic gains), and each dynamic band's range as a bracket. Draws
        nothing while no band is dynamic; repaints only when a dynamic gain
        moves. */
    class LiveCurve : public juce::Component
    {
    public:
        explicit LiveCurve (RKParaEQEditor& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void setGains (const std::array<float, ParaEQEngine::kBands>&);
        void paint (juce::Graphics&) override;

    private:
        RKParaEQEditor& owner;
        std::array<float, ParaEQEngine::kBands> gains {}, drawn {};
        float xFor (double hz) const;
        float yFor (double db) const;
    };

    /** The selected band's detector level against its threshold, and its
        dynamic gain right now. */
    class DynamicMeter : public juce::Component
    {
    public:
        explicit DynamicMeter (RKParaEQEditor& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void setValues (float levelDb, float gainDb, float secondsSinceLast);
        void paint (juce::Graphics&) override;

    private:
        RKParaEQEditor& owner;
        float shownLevel = -60.0f, shownGain = 0.0f;
    };

    RKParaEQProcessor& eq;
    ParaEQEngine::Settings settings;
    std::vector<std::unique_ptr<Strip>> strips;
    std::vector<std::unique_ptr<StripSelector>> selectors;
    std::array<DynamicControls, ParaEQEngine::kBands> dynamicControls;
    rkfx::ResponseCurve curve;
    LiveCurve liveCurve { *this };
    DynamicMeter meter { *this };
    rkfx::ParameterWatcher watcher;

    double lastMeterTime = 0.0;
    juce::TimedCallback meterTimer { [this] { updateMeters(); } };

    int selectedBand() const;
    void selectBand (int band);

    juce::Rectangle<int> curveArea() const;
    juce::Rectangle<int> stripArea (int band) const;
    juce::Rectangle<int> dynamicsArea() const;

    void onParameterChange();
    void updateMeters();

    void layoutContent() override;
    void paintContent (juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RKParaEQEditor)
};
