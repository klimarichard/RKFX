#pragma once

#include <juce_core/juce_core.h>

namespace rkfx
{

/** Where presets live on disk, shared by every RKFX plugin:

        <library>/Presets/<PluginName>/<name>.json      (sub-folders allowed)

    The library defaults to ~/Music/RKMusic Plugins/RKFX (under Music so
    iCloud's Desktop & Documents sync never moves it) and can be changed; the
    choice is stored as "libraryFolder" in
    ~/Library/Application Support/RKFX/settings.json. */
class PresetLibrary
{
public:
    /** @param settingsFile  injectable for tests; defaults to the real one. */
    explicit PresetLibrary (juce::File settingsFile = defaultSettingsFile());

    static juce::File defaultSettingsFile();
    static juce::File defaultLibraryFolder();

    /** Re-reads settings.json every call, so a change made in one plugin is
        seen by every other open instance. */
    juce::File getLibraryFolder() const;

    /** Stores the new location, keeping any other keys in settings.json. */
    bool setLibraryFolder (const juce::File& folder) const;

    juce::File getPresetFolder (const juce::String& pluginName) const;

    /** The file every new instance of the plugin starts with, if present. */
    juce::File getDefaultPresetFile (const juce::String& pluginName) const;

    /** Writes to a temporary file next to @p target, then swaps it in, so a
        failed write never damages an existing file. */
    static bool writeAtomically (const juce::File& target, const juce::String& text);

private:
    juce::File settingsFile;
};

} // namespace rkfx
