#include <juce_audio_processors/juce_audio_processors.h>

#include "../plugins/RKMix/PluginProcessor.h"
#include "../plugins/RKMix/MixInsertLayout.h"
#include "../plugins/RKGraphEQ/GraphEQLayout.h"
#include "host/PedalRegistry.h"

/** The iPad's layout of an insert: its window's controls in place, and a
    picture of the rest. Built into RKBoardTests, which links the plugins. */
class MixInsertLayoutTests : public juce::UnitTest
{
public:
    MixInsertLayoutTests() : juce::UnitTest ("RKMix insert layout") {}

    static juce::var parse (const juce::String& json) { return juce::JSON::parse (json); }

    static int count (const juce::var& layout, const juce::String& widget)
    {
        int n = 0;
        if (auto* list = layout["ctl"].getArray())
            for (const auto& c : *list)
                n += c["k"].toString() == widget ? 1 : 0;
        return n;
    }

    static const juce::var* find (const juce::var& layout, const juce::String& id)
    {
        if (auto* list = layout["ctl"].getArray())
            for (const auto& c : *list)
                if (c["id"].toString() == id)
                    return &c;
        return nullptr;
    }

    void runTest() override
    {
        rkfx::pedals::registerFactoryPresets();

        beginTest ("RKGraphEQ: its faders and the band switch where the window has them; the picture; the band count changes the layout");
        {
            RKMixProcessor mix;
            const InsertRack::Slot slot { InsertRack::Strip::channel, 2, 1 };
            auto* eq = mix.getRack().insert (slot, "RKGraphEQ", 1);
            expect (eq != nullptr);
            if (eq == nullptr)
                return;
            const auto flat = InsertRack::flatIndex (slot);
            const auto prefix = "ins:" + juce::String (flat) + ":";

            MixInsertLayout layout (*eq, flat);
            expect (layout.hasLayout());
            auto json = parse (layout.layoutJson());
            const auto bands = eq->findParam ("bands");
            const auto mode = juce::roundToInt (bands->convertFrom0to1 (bands->getValue()));
            expectEquals (count (json, "fader"), GraphEQParams::modeCount (mode));
            expect ((int) json["w"] > 400 && (int) json["h"] > 300);
            expect (json["img"].toString().startsWith ("data:image/png;base64,"));
            expectEquals (json["more"].size(), 0);
            expect (find (json, prefix + "bypass") == nullptr, "the bypass is the iPad's On");

            const auto* sw = find (json, prefix + "bands");
            expect (sw != nullptr);
            if (sw != nullptr)
            {
                expectEquals ((*sw)["k"].toString(), juce::String ("segments"));
                expectEquals ((*sw)["title"].toString(), juce::String ("Bands"));
                expectEquals ((*sw)["labels"].size(), 3);
            }
            // A band fader: its label (the frequency), its 0 dB in the middle, inside the window.
            if (auto* list = json["ctl"].getArray())
                for (const auto& c : *list)
                    if (c["k"].toString() == "fader")
                    {
                        expect (c["label"].toString().isNotEmpty());
                        expectWithinAbsoluteError ((double) c["zero"], 0.5, 0.01);
                        expect ((int) c["x"] >= 0 && (int) c["x"] + (int) c["w"] <= (int) json["w"]);
                        expect ((int) c["y"] >= 0 && (int) c["y"] + (int) c["h"] <= (int) json["h"]);
                    }

            expectEquals (layout.update(), juce::String(), "nothing changed");

            const auto other = mode == 0 ? 2 : 0;
            bands->setValueNotifyingHost (bands->convertTo0to1 ((float) other));
            expectEquals (layout.update(), juce::String ("layout"));
            json = parse (layout.layoutJson());
            expectEquals (count (json, "fader"), GraphEQParams::modeCount (other));
        }

        beginTest ("An insert about to be removed is told to the remote too (its hidden editor goes first)");
        {
            RKMixProcessor mix;
            const InsertRack::Slot slot { InsertRack::Strip::bus, 0, 0 };
            auto* plugin = mix.getRack().insert (slot, "RKCompress", 2);
            rkfx::ProcessorBase* told = nullptr;
            mix.insertAboutToRemoveRemote = [&told] (rkfx::ProcessorBase* p) { told = p; };
            mix.getRack().remove (slot);
            expect (plugin != nullptr && told == plugin);
            mix.insertAboutToRemoveRemote = nullptr;
        }

        beginTest ("Every insert: a layout, and every control reachable in it");
        {
            for (const auto& type : rkfx::pedals::pedalTypes())
            {
                auto plugin = type.create();
                plugin->setPlayConfigDetails (2, 2, 48000.0, 512);
                plugin->prepareToPlay (48000.0, 512);
                MixInsertLayout layout (*plugin, 0);
                expect (layout.hasLayout(), type.name);
                const auto json = parse (layout.layoutJson());
                juce::StringArray more;
                if (auto* list = json["more"].getArray())
                    for (const auto& id : *list)
                        more.add (id.toString());
                expect (more.isEmpty(), type.name + " has controls its window doesn't show: " + more.joinIntoString (", "));
                expect (json["ctl"].size() > 0, type.name);

                // RKMIX_LAYOUT_DUMP=<folder>: each plugin's picture and layout, to look at.
                if (const auto dump = juce::SystemStats::getEnvironmentVariable ("RKMIX_LAYOUT_DUMP", {}); dump.isNotEmpty())
                {
                    const juce::File folder (dump);
                    folder.createDirectory();
                    juce::MemoryOutputStream png;
                    juce::Base64::convertFromBase64 (png, json["img"].toString().fromFirstOccurrenceOf (",", false, false));
                    folder.getChildFile (type.name + ".png").replaceWithData (png.getData(), png.getDataSize());
                    folder.getChildFile (type.name + ".json").replaceWithText (layout.layoutJson().replace (json["img"].toString(), ""));
                }
                plugin->releaseResources();
            }
        }
    }
};

static MixInsertLayoutTests mixInsertLayoutTests;
