#pragma once

#include <juce_core/juce_core.h>

#include "GraphEQEngine.h"

#include <cmath>

/** Band set layout shared by the parameters, the editor and the tests:
    parameter IDs and the frequency labels under the sliders. */
namespace GraphEQParams
{
    inline constexpr GraphEQEngine::Mode modes[] = { GraphEQEngine::Mode::ten, GraphEQEngine::Mode::fifteen,
                                                     GraphEQEngine::Mode::thirtyOne };

    inline int modeCount (int modeIndex) { return GraphEQEngine::bandCount (modes[modeIndex]); }

    inline juce::String bandId (int modeIndex, int band)
    {
        return "g" + juce::String (modeCount (modeIndex)) + "_b" + juce::String (band + 1).paddedLeft ('0', 2);
    }

    /** "31.5", "250", "1k", "1.25k", "12.5k". */
    inline juce::String frequencyLabel (double hz)
    {
        const auto decimals = [] (double v)
        {
            const auto whole = [] (double x) { return std::abs (x - std::round (x)) < 1.0e-9; };
            return whole (v) ? 0 : whole (v * 10.0) ? 1 : 2;
        };

        if (hz >= 1000.0)
            return juce::String (hz / 1000.0, decimals (hz / 1000.0)) + "k";
        return juce::String (hz, decimals (hz));
    }
}
