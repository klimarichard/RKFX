#pragma once

#include <juce_core/juce_core.h>

#include <map>
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

/** Returns a plugin's factory presets, in menu order. */
using FactoryPresetSource = std::vector<FactoryPreset> (*)();

/** Factory preset sources by plugin name, so one binary can hold several
    plugins (RKBoard holds them all). Each plugin's list is a function
    rkfx::factoryPresetList_<Name>(), generated from plugins/<Name>/presets.
    Lists are registered by explicit calls, never by static initialisers:
    the plugin formats link the code from a static library, and the linker
    drops files that nothing refers to. A single plugin registers its own in
    ProcessorBase; RKBoard registers all of them (rkboard::registerFactoryPresets). */
inline std::map<juce::String, FactoryPresetSource>& factoryPresetRegistry()
{
    static std::map<juce::String, FactoryPresetSource> registry;
    return registry;
}

inline bool registerFactoryPresets (const juce::String& pluginName, FactoryPresetSource source)
{
    factoryPresetRegistry()[pluginName] = source;
    return true;
}

/** The factory presets of @p pluginName (none when it has no list). */
inline std::vector<FactoryPreset> factoryPresetsFor (const juce::String& pluginName)
{
    const auto& registry = factoryPresetRegistry();
    const auto it = registry.find (pluginName);
    return it != registry.end() ? it->second() : std::vector<FactoryPreset>{};
}

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
