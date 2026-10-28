#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/FactoryPresets.h"
#include "plugin/ParamBuilder.h"
#include "plugin/PresetFormat.h"

#include "../plugins/RKFreqShift/Parameters.h"
#include "../plugins/RKFilter/Parameters.h"
#include "../plugins/RKParaEQ/Parameters.h"
#include "../plugins/RKGraphEQ/Parameters.h"
#include "../plugins/RKCompress/Parameters.h"
#include "../plugins/RKGate/Parameters.h"
#include "../plugins/RKTremolo/Parameters.h"
#include "../plugins/RKChorus/Parameters.h"
#include "../plugins/RKDouble/Parameters.h"
#include "../plugins/RKFlange/Parameters.h"
#include "../plugins/RKPhase/Parameters.h"
#include "../plugins/RKDelay/Parameters.h"
#include "../plugins/RKSaturate/Parameters.h"
#include "../plugins/RKDistort/Parameters.h"
#include "../plugins/RKVerb/Parameters.h"
#include "../plugins/RKLimit/Parameters.h"
#include "../plugins/RKTransient/Parameters.h"
#include "../plugins/RKDeEss/Parameters.h"
#include "../plugins/RKSwell/Parameters.h"
#include "../plugins/RKRotary/Parameters.h"
#include "../plugins/RKVibrato/Parameters.h"
#include "../plugins/RKVocode/Parameters.h"
#include "../plugins/RKUtility/Parameters.h"
#include "../plugins/RKOctave/Parameters.h"
#include "../plugins/RKAcoustic/Parameters.h"
#include "../plugins/RKPreamp/Parameters.h"
#include "../plugins/RKSynth/Parameters.h"
#include "../plugins/RKRingMod/Parameters.h"
#include "../plugins/RKLoop/Parameters.h"
#include "../plugins/RKHarmonize/Parameters.h"
#include "../plugins/RKArpeggiate/Parameters.h"
#include "../plugins/RKStrum/Parameters.h"
#include "../plugins/RKSustain/Parameters.h"
#include "../plugins/RKConduct/Parameters.h"

#include <cstdio>
#include <functional>

/** Checks every factory preset file against its plugin's real parameters:
    right plugin and version, and every key and value understood (a preset
    that loads with a note means a typo in an ID or a value). */
class FactoryPresetTests : public juce::UnitTest
{
public:
    FactoryPresetTests() : juce::UnitTest ("RKFX factory presets") {}

    void runTest() override
    {
        beginTest ("Display names drop the ordering prefix");
        {
            expectEquals (rkfx::factoryPresetName ("03 Classic Jet.json"), juce::String ("Classic Jet"));
            expectEquals (rkfx::factoryPresetName ("Plain.json"), juce::String ("Plain"));
            expectEquals (rkfx::factoryPresetName ("10cc Sound.json"), juce::String ("10cc Sound"));
        }

        struct Plugin { const char* name; std::function<rkfx::ParamBuilder()> create; int minimum; };
        const Plugin plugins[] = {
            { "RKFreqShift", FreqShiftParams::create, 5 }, { "RKFilter", FilterParams::create, 6 },
            { "RKParaEQ", ParaEQParams::create, 5 },       { "RKGraphEQ", GraphEQParams::create, 4 },
            { "RKCompress", CompressParams::create, 5 },   { "RKGate", GateParams::create, 5 },
            { "RKTremolo", TremoloParams::create, 6 },     { "RKChorus", ChorusParams::create, 6 },
            { "RKDouble", DoubleParams::create, 5 },       { "RKFlange", FlangeParams::create, 6 },
            { "RKPhase", PhaseParams::create, 8 },         { "RKDelay", DelayParams::create, 7 },
            { "RKSaturate", SaturateParams::create, 6 },   { "RKDistort", DistortParams::create, 7 },
            { "RKVerb", VerbParams::create, 8 },        { "RKLimit", LimitParams::create, 6 },
            { "RKTransient", TransientParams::create, 7 }, { "RKDeEss", DeEssParams::create, 7 },
            { "RKSwell", SwellParams::create, 6 },       { "RKRotary", RotaryParams::create, 7 },
            { "RKVibrato", VibratoParams::create, 6 },   { "RKVocode", VocodeParams::create, 7 },
            { "RKUtility", UtilityParams::create, 7 },   { "RKOctave", OctaveParams::create, 6 },
            { "RKAcoustic", AcousticParams::create, 6 }, { "RKPreamp", PreampParams::create, 7 },
            { "RKSynth", SynthParams::create, 6 },       { "RKRingMod", RingModParams::create, 6 },
            { "RKLoop", LoopParams::create, 8 },        { "RKHarmonize", HarmonizeParams::create, 6 },
            { "RKArpeggiate", ArpParams::create, 6 },    { "RKStrum", StrumParams::create, 5 },
            { "RKSustain", SustainParams::create, 5 },   { "RKConduct", ConductParams::create, 7 },
        };

        for (const auto& plugin : plugins)
        {
            beginTest (juce::String (plugin.name) + " factory presets load cleanly");

            // The plugin's own parameters plus the standard ones a preset may set.
            auto builder = plugin.create();
            builder.addPercent ("mix", "Dry/Wet", 0.0f, 100.0f, 100.0f);
            builder.addDecibels ("output", "Output", -24.0f, 12.0f, 0.0f);

            std::vector<rkfx::ParamInfo> infos;
            for (const auto& p : builder.params)
                infos.push_back (rkfx::describe (*p));

            const auto folder = juce::File (RKFX_SOURCE_DIR).getChildFile ("plugins").getChildFile (plugin.name).getChildFile ("presets");
            auto files = folder.findChildFiles (juce::File::findFiles, false, "*.json");
            expect (files.size() >= plugin.minimum, juce::String (files.size()) + " presets");

            juce::StringArray names;
            for (const auto& f : files)
            {
                const auto r = rkfx::PresetFormat::read (f.loadFileAsString(), plugin.name, 1, infos);
                expect (r.ok, f.getFileName() + ": " + r.error);
                expect (r.notes.isEmpty(), f.getFileName() + ": " + r.notes.joinIntoString ("; "));
                expectEquals (r.version, 1);
                expect (r.name == rkfx::factoryPresetName (f.getFileName()), f.getFileName() + ": name matches the file");
                expect (! names.contains (r.name), "unique name");
                names.add (r.name);
            }
        }
    }
};

static FactoryPresetTests factoryPresetTests;

int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i))
            failures += r->failures;

    std::printf (failures > 0 ? "RKFXPresetTests: %d failure(s)\n" : "RKFXPresetTests: all presets valid\n", failures);
    return failures > 0 ? 1 : 0;
}
