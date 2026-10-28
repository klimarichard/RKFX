#pragma once

#include "plugin/ProcessorBase.h"

#include <memory>
#include <vector>

namespace rkboard
{

/** One pedal type the board can hold: every RKFX plugin, built in. */
struct PedalType
{
    juce::String name;                         // plugin name, e.g. "RKChorus" (also the preset folder)
    juce::String shortName;                    // tile label, e.g. "Chorus"
    rkfx::Palette::Family family;
    std::unique_ptr<rkfx::ProcessorBase> (*create)();
    double tailSeconds = 0.0;                  // how long it rings on after the input stops
};

/** Every pedal type, grouped by family in menu order. */
const std::vector<PedalType>& pedalTypes();

/** The type called @p name, or nullptr. */
const PedalType* findPedalType (const juce::String& name);

/** Menu heading for a family ("Filter and EQ", ...). */
juce::String familyName (rkfx::Palette::Family);

} // namespace rkboard
