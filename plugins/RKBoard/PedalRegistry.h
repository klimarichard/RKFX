#pragma once

#include "host/PedalRegistry.h"

namespace rkfx { std::vector<FactoryPreset> factoryPresetList_RKBoard(); }

namespace rkboard
{

// The pedals come from the shared pedal host (host/), as RKMix's inserts do.
using rkfx::pedals::PedalType;
using rkfx::pedals::pedalTypes;
using rkfx::pedals::findPedalType;
using rkfx::pedals::familyName;

/** Registers every pedal's factory presets and the board's own. Calls after
    the first do nothing. */
inline void registerFactoryPresets()
{
    rkfx::pedals::registerFactoryPresets();
    static const bool registered = rkfx::registerFactoryPresets ("RKBoard", &rkfx::factoryPresetList_RKBoard);
    juce::ignoreUnused (registered);
}

} // namespace rkboard
