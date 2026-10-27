#pragma once

#include "plugin/ParamBuilder.h"

/** RKFreqShift parameter IDs and ranges. IDs are permanent: never rename,
    reuse or reorder; only append. */
namespace FreqShiftParams
{
    inline constexpr int kNumShifts = 4;

    inline juce::String id (int shift, const char* name)
    {
        return "shift" + juce::String (shift + 1) + "_" + name;
    }

    /** Coarse range: 0 to 5 kHz on an exponential curve (100 Hz sits a
        little before the middle), so the low end keeps fine resolution. */
    inline juce::NormalisableRange<float> coarseRange()
    {
        constexpr float maxHz = 5000.0f, curve = 7.0f;
        const auto scale = maxHz / (std::exp (curve) - 1.0f);

        return { 0.0f, maxHz,
                 [=] (float, float, float x) { return scale * (std::exp (curve * x) - 1.0f); },
                 [=] (float, float, float v) { return std::log (v / scale + 1.0f) / curve; },
                 [] (float lo, float hi, float v) { return juce::jlimit (lo, hi, v); } };
    }

    inline rkfx::ParamBuilder create()
    {
        rkfx::ParamBuilder p;

        for (int s = 0; s < kNumShifts; ++s)
        {
            const auto name = "Shift " + juce::String (s + 1) + " ";

            p.addBool    (id (s, "on"), name + "On", s == 0);
            p.addFloat   (id (s, "freq"), name + "Frequency", coarseRange(), 100.0f,
                          rkfx::Units::frequency, rkfx::Units::parseFrequency);
            p.addChoice  (id (s, "range"), name + "Range", { "Fine", "Coarse" }, { "fine", "coarse" }, 1);
            p.addPercent (id (s, "updown"), name + "Up/Down", 0.0f, 100.0f, 0.0f);
            p.addPercent (id (s, "feedback"), name + "Feedback", 0.0f, 90.0f, 0.0f);
            p.addPercent (id (s, "level"), name + "Level", 0.0f, 100.0f, 100.0f);
            p.addFloat   (id (s, "fine"), name + "Fine Frequency", { 0.0f, 50.0f }, 10.0f,
                          rkfx::Units::frequency, rkfx::Units::parseFrequency);
        }

        p.addPercent   ("spread", "Stereo Spread", 0.0f, 100.0f, 0.0f);
        p.addFrequency ("lowcut", "Low Cut", 20.0f, 1000.0f, 20.0f);
        p.addFrequency ("highcut", "High Cut", 1000.0f, 20000.0f, 20000.0f);

        return p;
    }
}
