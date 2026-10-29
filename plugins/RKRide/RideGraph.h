#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "RideEngine.h"

#include <array>

/** RKRide's main display: the last 6 seconds, scrolling right to left.

    - Upper lane, level (-60 to 0 dB): the weighted input level (a filled
      area, gray while below the Floor), the Target (a line, following the
      music in Relative mode) and the Floor (dotted).
    - Lower lane, gain (+-18 dB around 0): the Max Boost / Max Cut range as a
      band and the gain as a bright line (scaled to that range).

    Fed by the editor's ~30 fps timer, only while audio is processed. */
class RideGraph : public juce::Component
{
public:
    explicit RideGraph (juce::Colour accent);

    void push (const RideEngine::Meter&);
    void setSettings (const RideEngine::Settings&);

    void paint (juce::Graphics&) override;

    static constexpr int kPoints = 180;   // 6 s at 30 per second

private:
    struct Point
    {
        float levelDb = -120.0f, targetDb = -18.0f, gainDb = 0.0f;
        bool holding = true;
    };

    juce::Colour accent;
    RideEngine::Settings settings;
    std::array<Point, kPoints> history {};
    int next = 0;   // oldest entry, overwritten next

    const Point& at (int age) const;   // 0 = oldest
};

/** A fader cap showing the gain right now (+-18 dB), moving like a real
    fader. Display only. */
class RideFader : public juce::Component
{
public:
    explicit RideFader (juce::Colour accent);

    void setGain (float gainDb);
    void paint (juce::Graphics&) override;

private:
    juce::Colour accent;
    float shown = 0.0f;
};
