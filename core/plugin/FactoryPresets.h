#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace rkfx
{

/** A preset compiled into the plugin (read-only), shown in the Load menu
    under "Factory". */
struct FactoryPreset
{
    juce::String name;   // display name (file name without the "NN " ordering prefix)
    juce::String json;   // same format as a preset file
};

/** The plugin's factory presets, in menu order. Defined per plugin by a file
    rkfx_add_plugin() generates from the JSON files in plugins/<Name>/presets. */
std::vector<FactoryPreset> getFactoryPresets();

/** "03 Classic Jet.json" -> "Classic Jet". */
inline juce::String factoryPresetName (const juce::String& fileName)
{
    auto name = fileName.upToLastOccurrenceOf (".json", false, true).trim();
    const auto prefix = name.initialSectionContainingOnly ("0123456789");
    if (prefix.isNotEmpty() && name.substring (prefix.length()).startsWithChar (' '))
        name = name.substring (prefix.length()).trimStart();
    return name;
}

} // namespace rkfx
