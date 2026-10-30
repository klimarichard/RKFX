#pragma once

#include "plugin/ProcessorBase.h"

#include <memory>
#include <vector>

namespace rkfx::pedals
{

/** One pedal type RKBoard and RKMix can hold: every RKFX audio plugin, built in. */
struct PedalType
{
    juce::String name;                         // plugin name, e.g. "RKChorus" (also the preset folder)
    juce::String shortName;                    // tile label, e.g. "Chorus"
    rkfx::Palette::Family family;
    std::unique_ptr<rkfx::ProcessorBase> (*create)();
    double tailSeconds = 0.0;                  // how long it rings on after the input stops
    bool sendsMidi = false;                    // sends MIDI (RKTrigger, RKFollow): RKMix's inserts only, RKBoard has no MIDI out
};

/** Every pedal type, grouped by family in menu order. */
const std::vector<PedalType>& pedalTypes();

/** Registers the factory presets of every pedal type. Call it before
    creating a pedal; calls after the first do nothing. */
void registerFactoryPresets();

/** The type called @p name, or nullptr. */
const PedalType* findPedalType (const juce::String& name);

/** Menu heading for a family ("Filter and EQ", ...). */
juce::String familyName (rkfx::Palette::Family);

} // namespace rkfx::pedals
