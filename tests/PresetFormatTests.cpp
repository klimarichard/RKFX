#include <juce_core/juce_core.h>

#include "plugin/PresetFormat.h"

using namespace rkfx;

class PresetFormatTests : public juce::UnitTest
{
public:
    PresetFormatTests() : juce::UnitTest ("RKFX preset format") {}

    void runTest() override
    {
        const auto infos = makeInfos();

        beginTest ("Round trip: save then load gives identical values");
        {
            ParamValues values { { "rate", 0.6 }, { "depth", 55.0 }, { "voices", 3.0 },
                                 { "mode", 2.0 }, { "sync", 1.0 }, { "freq", 1234.5 } };

            for (const bool onlyNonDefault : { true, false })
            {
                const auto json = PresetFormat::toJson (PresetFormat::toVar ("RKTest", 1, {}, infos, values, onlyNonDefault));
                const auto r = PresetFormat::read (json, "RKTest", 1, infos);

                expect (r.ok, r.error);
                expect (r.notes.isEmpty(), r.notes.joinIntoString ("; "));
                for (const auto& [id, v] : values)
                    expectWithinAbsoluteError (r.values.at (id), v, 1.0e-6);
                expectEquals (r.values.at ("output"), 0.0, "missing value takes its default");
            }
        }

        beginTest ("Only non-default values are written, in readable units");
        {
            ParamValues values = PresetFormat::defaults (infos);
            values["rate"] = 0.6f;          // float noise must not reach the file
            values["mode"] = 2.0;
            values["sync"] = 1.0;
            values["voices"] = 3.0;

            const auto v = PresetFormat::toVar ("RKTest", 1, {}, infos, values, true);
            const auto* params = v["params"].getDynamicObject();

            expectEquals (params->getProperties().size(), 4);
            expectEquals (v["params"]["rate"].toString(), juce::String ("0.6"));
            expectEquals (v["params"]["mode"].toString(), juce::String ("bandpass"));
            expect (v["params"]["sync"].isBool() && (bool) v["params"]["sync"]);
            expect (v["params"]["voices"].isInt());
            expectEquals (v["plugin"].toString(), juce::String ("RKTest"));
            expectEquals ((int) v["version"], 1);
            expect (! v.hasProperty ("name"), "name is optional and omitted when empty");
        }

        beginTest ("A preset for another plugin, or not a preset, is refused");
        {
            expect (! PresetFormat::read (R"({"plugin":"RKOther","version":1,"params":{}})", "RKTest", 1, infos).ok);
            expect (! PresetFormat::read ("not json", "RKTest", 1, infos).ok);
            expect (! PresetFormat::read ("[1,2]", "RKTest", 1, infos).ok);
            expect (! PresetFormat::read (R"({"plugin":"RKTest","version":1})", "RKTest", 1, infos).ok);
        }

        beginTest ("Bad values never fail the load");
        {
            const auto r = PresetFormat::read (R"({
                "plugin": "RKTest", "version": 1, "name": "Wide / Keys",
                "params": {
                    "rate": "fast",
                    "depth": 250,
                    "mode": "sideways",
                    "sync": true,
                    "unknown_param": 3,
                    "voices": 2
                }
            })", "RKTest", 1, infos);

            expect (r.ok);
            expectEquals (r.name, juce::String ("Wide / Keys"));
            expectEquals (r.notes.size(), 4);
            expectEquals (r.values.at ("rate"), 0.8, "invalid -> default");
            expectEquals (r.values.at ("depth"), 100.0, "out of range -> clamped");
            expectEquals (r.values.at ("mode"), 0.0, "unknown choice -> default");
            expectEquals (r.values.at ("sync"), 1.0);
            expectEquals (r.values.at ("voices"), 2.0);
        }

        beginTest ("Older versions are migrated before validation");
        {
            int migratedFrom = -1;
            const auto r = PresetFormat::read (R"({"plugin":"RKTest","version":0,"params":{"speed":0.3}})",
                                               "RKTest", 1, infos,
                                               [&] (juce::DynamicObject& params, int from)
                                               {
                                                   migratedFrom = from;
                                                   params.setProperty ("rate", params.getProperty ("speed"));
                                                   params.removeProperty ("speed");
                                               });
            expect (r.ok);
            expectEquals (migratedFrom, 0);
            expectWithinAbsoluteError (r.values.at ("rate"), 0.3, 1.0e-9);
            expect (r.notes.isEmpty());
        }

        beginTest ("Host state keeps the modified flag");
        {
            auto v = PresetFormat::toVar ("RKTest", 1, "Lead", infos, PresetFormat::defaults (infos), false);
            v.getDynamicObject()->setProperty ("modified", true);
            const auto r = PresetFormat::read (PresetFormat::toJson (v), "RKTest", 1, infos);
            expect (r.ok && r.modified);
            expectEquals (r.name, juce::String ("Lead"));
        }
    }

private:
    static std::vector<ParamInfo> makeInfos()
    {
        using K = ParamInfo::Kind;
        std::vector<ParamInfo> v;
        v.push_back ({ "rate",   K::Float,  0.05, 10.0,    0.8,  {} });
        v.push_back ({ "depth",  K::Float,  0.0,  100.0,   40.0, {} });
        v.push_back ({ "freq",   K::Float,  20.0, 20000.0, 1000.0, {} });
        v.push_back ({ "voices", K::Int,    1.0,  4.0,     1.0,  {} });
        v.push_back ({ "mode",   K::Choice, 0.0,  2.0,     0.0,  { "lowpass", "highpass", "bandpass" } });
        v.push_back ({ "sync",   K::Bool,   0.0,  1.0,     0.0,  {} });
        v.push_back ({ "output", K::Float, -24.0, 12.0,    0.0,  {} });
        return v;
    }
};

static PresetFormatTests presetFormatTests;
