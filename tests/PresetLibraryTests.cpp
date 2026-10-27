#include <juce_core/juce_core.h>

#include "plugin/PresetLibrary.h"

using namespace rkfx;

class PresetLibraryTests : public juce::UnitTest
{
public:
    PresetLibraryTests() : juce::UnitTest ("RKFX preset library") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("RKFXTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        root.createDirectory();
        const auto settings = root.getChildFile ("settings.json");

        beginTest ("Defaults to ~/Music/RKMusic Plugins/RKFX");
        {
            PresetLibrary library (settings);
            expect (library.getLibraryFolder() == PresetLibrary::defaultLibraryFolder());
            expect (PresetLibrary::defaultLibraryFolder().getFullPathName().endsWith ("Music/RKMusic Plugins/RKFX"));
            expect (library.getPresetFolder ("RKChorus") == PresetLibrary::defaultLibraryFolder().getChildFile ("Presets/RKChorus"));
        }

        beginTest ("Library location is stored and other settings are kept");
        {
            settings.replaceWithText (R"({"somethingElse": 42})");

            PresetLibrary library (settings);
            const auto custom = root.getChildFile ("My Library");
            expect (library.setLibraryFolder (custom));

            expect (PresetLibrary (settings).getLibraryFolder() == custom, "seen by another instance");
            expect (library.getDefaultPresetFile ("RKDelay") == custom.getChildFile ("Presets/RKDelay/Default.json"));

            const auto json = juce::JSON::parse (settings);
            expectEquals ((int) json["somethingElse"], 42);
        }

        beginTest ("Atomic write replaces the file");
        {
            const auto file = root.getChildFile ("preset.json");
            expect (PresetLibrary::writeAtomically (file, "first"));
            expect (PresetLibrary::writeAtomically (file, "second"));
            expectEquals (file.loadFileAsString(), juce::String ("second"));
            expectEquals (root.findChildFiles (juce::File::findFiles, false, "*.tmp").size(), 0);
        }

        root.deleteRecursively();
    }
};

static PresetLibraryTests presetLibraryTests;
