#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "MultiCompEngine.h"

#include <array>
#include <functional>

/** The RKMultiComp display: frequency (20 Hz to 20 kHz, logarithmic, as
    RKParaEQ's curve) across, level (-60 to 0 dBFS) up.

    - crossovers in use as vertical lines, each band's threshold (with
      Offset) as a horizontal line across its band;
    - a live column per band: its level, with the part the compressor is
      taking away in the accent colour, and the reduction in dB.

    Three layers: the grid and the lines are cached images, redrawn only in
    refresh() (on a parameter change); only the columns repaint live, at the
    editor's meter rate.

    Threshold lines drag up and down, crossover lines sideways (Shift for
    fine, double-click resets, as on the knobs); a click elsewhere selects
    the band under the mouse. */
class BandDisplay : public juce::Component
{
public:
    struct Parameters
    {
        std::array<juce::RangedAudioParameter*, MultiCompEngine::kNumBands> thresholds {};
        juce::RangedAudioParameter *low = nullptr, *mid = nullptr, *high = nullptr;
    };

    BandDisplay (std::function<MultiCompEngine::Settings()> settings, Parameters, juce::Colour accent,
                 std::function<void (MultiCompEngine::Band)> onSelect);

    /** Re-reads the settings and redraws the lines. */
    void refresh();

    void setSelectedBand (MultiCompEngine::Band);

    /** Feeds the columns (from a ~30 fps timer). */
    void setMeter (MultiCompEngine::Band, MultiCompEngine::Meter, float secondsSinceLast);

    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr float kMinDb = -60.0f;

private:
    struct Layer : public juce::Component
    {
        Layer (BandDisplay& o, void (BandDisplay::*p) (juce::Graphics&)) : owner (o), painter (p)
        {
            setInterceptsMouseClicks (false, false);
        }
        void paint (juce::Graphics& g) override { (owner.*painter) (g); }
        BandDisplay& owner;
        void (BandDisplay::*painter) (juce::Graphics&);
    };

    std::function<MultiCompEngine::Settings()> getSettings;
    Parameters params;
    juce::Colour accent;
    std::function<void (MultiCompEngine::Band)> onSelect;

    MultiCompEngine::Settings settings;
    std::array<MultiCompEngine::Band, MultiCompEngine::kNumBands> bandList {};
    int numBands = 3;
    MultiCompEngine::Band selected = MultiCompEngine::low;

    std::array<float, MultiCompEngine::kNumBands> shownLevel {}, shownReduction {};

    Layer grid { *this, &BandDisplay::paintGrid };
    Layer columns { *this, &BandDisplay::paintColumns };
    Layer lines { *this, &BandDisplay::paintLines };

    // Dragging
    enum class Target { none, threshold, crossover };
    struct Hit
    {
        Target target = Target::none;
        juce::RangedAudioParameter* param = nullptr;
        MultiCompEngine::Band band = MultiCompEngine::low;
    };
    Hit dragging;
    float lastDragPos = 0.0f;

    float xFor (double hz) const;
    double hzFor (float x) const;
    float yFor (float db) const;

    /** The band's region, left and right edges in pixels. */
    juce::Range<float> regionOf (int index) const;
    int regionAt (float x) const;
    std::array<float, MultiCompEngine::kNumBands - 1> crossoverHz() const;

    Hit findTarget (juce::Point<float>) const;

    void paintGrid (juce::Graphics&);
    void paintColumns (juce::Graphics&);
    void paintLines (juce::Graphics&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandDisplay)
};
