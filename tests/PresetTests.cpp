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
#include "../plugins/RKVoice/Parameters.h"
#include "../plugins/RKMultiComp/Parameters.h"
#include "../plugins/RKRide/Parameters.h"
#include "../plugins/RKFollow/Parameters.h"
#include "../plugins/RKTrigger/Parameters.h"
#include "../plugins/RKHold/Parameters.h"
#include "../plugins/RKEcho/Parameters.h"
#include "../plugins/RKPolyPitch/Parameters.h"
#include "../plugins/RKRemap/Parameters.h"
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

        struct Plugin { const char* name; std::function<rkfx::ParamBuilder()> create; int minimum; int stateVersion = 1; };
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
            { "RKLoop", LoopParams::create, 8 },        { "RKVoice", VoiceParams::create, 7, 2 },
            { "RKMultiComp", MultiCompParams::create, 6 }, { "RKRide", RideParams::create, 6 },
            { "RKFollow", FollowParams::create, 6 },       { "RKTrigger", TriggerParams::create, 6 },
            { "RKHold", HoldParams::create, 6 },          { "RKEcho", EchoParams::create, 6 },
            { "RKPolyPitch", PolyPitchParams::create, 7 }, { "RKRemap", RemapParams::create, 4 },
            { "RKHarmonize", HarmonizeParams::create, 7, 2 },
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
                const auto r = rkfx::PresetFormat::read (f.loadFileAsString(), plugin.name, plugin.stateVersion, infos);
                expect (r.ok, f.getFileName() + ": " + r.error);
                expect (r.notes.isEmpty(), f.getFileName() + ": " + r.notes.joinIntoString ("; "));
                expect (r.version >= 1 && r.version <= plugin.stateVersion, f.getFileName() + ": version " + juce::String (r.version));
                expect (r.name == rkfx::factoryPresetName (f.getFileName()), f.getFileName() + ": name matches the file");
                expect (! names.contains (r.name), "unique name");
                names.add (r.name);
            }
        }
    }
};

static FactoryPresetTests factoryPresetTests;

/** The suite-wide Hz and ms/s readouts, at every rounding boundary. */
class UnitsTests : public juce::UnitTest
{
public:
    UnitsTests() : juce::UnitTest ("Units readouts", "RKFX") {}

    void runTest() override
    {
        using namespace rkfx;

        beginTest ("Frequency");
        const std::pair<float, const char*> hz[] = {
            { 0.0f, "0 Hz" },        { 0.02f, "0.02 Hz" },    { 0.994f, "0.99 Hz" },  { 0.996f, "1.0 Hz" },
            { 6.5f, "6.5 Hz" },      { 9.94f, "9.9 Hz" },     { 9.96f, "10 Hz" },     { 10.4f, "10 Hz" },
            { 133.4f, "133 Hz" },    { 950.0f, "950 Hz" },    { 999.4f, "999 Hz" },   { 999.6f, "1.00 kHz" },
            { 1000.0f, "1.00 kHz" }, { 1620.0f, "1.62 kHz" }, { 9994.0f, "9.99 kHz" }, { 9996.0f, "10.0 kHz" },
            { 11300.0f, "11.3 kHz" }, { 16840.0f, "16.8 kHz" }, { 20000.0f, "20.0 kHz" } };
        for (const auto& [v, text] : hz)
            expectEquals (Units::frequency (v), juce::String (text));

        beginTest ("Time");
        const std::pair<float, const char*> ms[] = {
            { 0.0f, "0 ms" },          { 0.01f, "0.01 ms" },     { 0.994f, "0.99 ms" },  { 0.996f, "1.0 ms" },
            { 1.44f, "1.4 ms" },       { 9.94f, "9.9 ms" },      { 9.96f, "10 ms" },     { 10.44f, "10 ms" },
            { 375.0f, "375 ms" },      { 999.4f, "999 ms" },     { 999.6f, "1.00 s" },   { 1625.5f, "1.63 s" },
            { 9994.0f, "9.99 s" },     { 9996.0f, "10.0 s" },    { 10040.0f, "10.0 s" }, { 99940.0f, "99.9 s" },
            { 99960.0f, "100 s" },     { 123456.0f, "123 s" } };
        for (const auto& [v, text] : ms)
            expectEquals (Units::time (v), juce::String (text));

        beginTest ("Readouts parse back");
        for (const auto& [v, text] : hz)
            expectWithinAbsoluteError (Units::parseFrequency (Units::frequency (v)), v, 0.006f + v * 0.05f);
        for (const auto& [v, text] : ms)
            expectWithinAbsoluteError (Units::parseTime (Units::time (v)), v, 0.006f + v * 0.05f);
    }
};

static UnitsTests unitsTests;

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
