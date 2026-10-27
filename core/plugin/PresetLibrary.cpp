#include "PresetLibrary.h"

namespace rkfx
{

PresetLibrary::PresetLibrary (juce::File settings) : settingsFile (std::move (settings)) {}

juce::File PresetLibrary::defaultSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support/RKFX/settings.json");
}

juce::File PresetLibrary::defaultLibraryFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("RKMusic Plugins/RKFX");
}

juce::File PresetLibrary::getLibraryFolder() const
{
    const auto settings = juce::JSON::parse (settingsFile);
    const auto path = settings["libraryFolder"].toString();

    if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
        return juce::File (path);

    return defaultLibraryFolder();
}

bool PresetLibrary::setLibraryFolder (const juce::File& folder) const
{
    auto settings = juce::JSON::parse (settingsFile);
    if (settings.getDynamicObject() == nullptr)
        settings = juce::var (new juce::DynamicObject());

    settings.getDynamicObject()->setProperty ("libraryFolder", folder.getFullPathName());

    if (! settingsFile.getParentDirectory().createDirectory())
        return false;

    return writeAtomically (settingsFile, juce::JSON::toString (settings, false) + "\n");
}

juce::File PresetLibrary::getPresetFolder (const juce::String& pluginName) const
{
    return getLibraryFolder().getChildFile ("Presets").getChildFile (pluginName);
}

juce::File PresetLibrary::getDefaultPresetFile (const juce::String& pluginName) const
{
    return getPresetFolder (pluginName).getChildFile ("Default.json");
}

bool PresetLibrary::writeAtomically (const juce::File& target, const juce::String& text)
{
    juce::TemporaryFile temp (target);

    if (! temp.getFile().replaceWithText (text, false, false, "\n"))
        return false;

    return temp.overwriteTargetFileWithTemporary();
}

} // namespace rkfx
