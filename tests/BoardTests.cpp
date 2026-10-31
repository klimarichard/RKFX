// RKBoard: the board processor with real pedals (every RKFX plugin built in).

#include "BoardEditor.h"
#include "PitchDetector.h"
#include "pedals/VolumeEngine.h"
#include "pedals/WahEngine.h"
#include "../RKLoop/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

using rkboard::BoardProcessor;
using rkboard::Section;
using rkboard::chainSection;
using rkboard::pathSection;

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 256;

    void prepare (BoardProcessor& board)
    {
        board.setPlayConfigDetails (2, 2, kRate, kBlock);
        board.prepareToPlay (kRate, kBlock);
    }

    void fillNoise (juce::AudioBuffer<float>& buffer, juce::Random& random)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, (random.nextFloat() * 2.0f - 1.0f) * 0.5f);
    }

    /** Runs @p blocks blocks of noise; returns the last input and output. */
    std::pair<juce::AudioBuffer<float>, juce::AudioBuffer<float>> run (BoardProcessor& board, int blocks)
    {
        juce::Random random (42);
        juce::AudioBuffer<float> in (2, kBlock), out (2, kBlock);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            fillNoise (in, random);
            out.makeCopyOf (in);
            board.processBlock (out, midi);
        }
        return { in, out };
    }

    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    return false;
        return true;
    }

    bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (! juce::exactlyEqual (a.getSample (ch, i), b.getSample (ch, i)))
                    return false;
        return true;
    }

    juce::String stateJson (BoardProcessor& board)
    {
        juce::MemoryBlock block;
        board.getStateInformation (block);
        return block.toString();
    }
}

class BoardTests : public juce::UnitTest
{
public:
    BoardTests() : juce::UnitTest ("RKBoard") {}

    void runTest() override
    {
        beginTest ("Empty board passes the input through unchanged");
        {
            BoardProcessor board;
            board.clearBoard();
            prepare (board);
            const auto [in, out] = run (board, 8);
            expect (identical (in, out));
        }

        beginTest ("Every pedal type can be added, processes, and has its factory presets");
        {
            for (const auto& type : rkboard::pedalTypes())
            {
                BoardProcessor board;
                board.clearBoard();
                prepare (board);
                if (type.sendsMidi)
                {
                    expectEquals (board.addPedal (type.name), 0, type.name + ": not on the board (no MIDI out)");
                    continue;
                }
                const auto id = board.addPedal (type.name);
                expect (id == 1, type.name);

                auto* pedal = board.getPedal (id);
                expect (pedal != nullptr && pedal->getConfig().name == type.name, type.name);

                const auto [in, out] = run (board, 40);
                expect (allFinite (out), type.name + " output is finite");

                // Plugins keep theirs in plugins/<Name>/presets, the host's own
                // pedals in host/pedals/presets/<Name>.
                const auto source = juce::File (RKFX_SOURCE_DIR);
                auto presetDir = source.getChildFile ("plugins/" + type.name + "/presets");
                if (! presetDir.isDirectory())
                    presetDir = source.getChildFile ("host/pedals/presets/" + type.name);
                const auto files = presetDir.findChildFiles (juce::File::findFiles, false, "*.json");
                expect (! files.isEmpty(), type.name + " has factory presets");
                expectEquals ((int) pedal->getFactoryPresets().size(), files.size(), type.name + " factory presets");

                for (int i = 0; i < (int) pedal->getFactoryPresets().size(); ++i)
                {
                    const auto r = pedal->loadFactoryPreset (i);
                    expect (r.ok && r.message.isEmpty(), type.name + " preset " + juce::String (i) + ": " + r.message);
                }
            }
        }

        beginTest ("Bypassed pedals (no latency) give back the input exactly");
        {
            BoardProcessor board;
            board.clearBoard();
            prepare (board);
            for (const auto* name : { "RKChorus", "RKCompress", "RKDelay", "RKVerb", "RKParaEQ", "RKPhase" })
                board.setPedalBypassed (board.addPedal (name), true);

            run (board, 100);   // past any bypass crossfade
            const auto [in, out] = run (board, 1);
            expect (identical (in, out));
        }

        beginTest ("Board is full at 64 pedals");
        {
            BoardProcessor board;
            board.clearBoard();
            for (int i = 0; i < BoardProcessor::kMaxPedals; ++i)
                expect (board.addPedal ("RKUtility") == i + 1);
            expectEquals (board.addPedal ("RKUtility"), 0);
            expectEquals (board.duplicatePedal (1), 0);
            expectEquals (board.addPedal ("NoSuchPedal"), 0);
        }

        beginTest ("Move, duplicate and remove keep pedal IDs");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto a = board.addPedal ("RKChorus");
            const auto b = board.addPedal ("RKDelay");
            const auto c = board.addPedal ("RKVerb");
            expect (board.getChain() == std::vector<int> { a, b, c });

            board.movePedal (c, Section::pre, 0);
            expect (board.getChain() == std::vector<int> { c, a, b });
            board.movePedal (c, Section::pre, 3);   // to the end
            expect (board.getChain() == std::vector<int> { a, b, c });
            board.movePedal (a, Section::pre, 2);   // between b and c
            expect (board.getChain() == std::vector<int> { b, a, c });

            board.getPedal (a)->findParam ("rate")->setValueNotifyingHost (0.8f);
            const auto copy = board.duplicatePedal (a);
            expectEquals (copy, 4);
            expect (board.getChain() == std::vector<int> { b, a, copy, c });
            expectWithinAbsoluteError (board.getPedal (copy)->findParam ("rate")->getValue(),
                                       board.getPedal (a)->findParam ("rate")->getValue(), 1.0e-5f);

            board.removePedal (b);
            expect (board.getChain() == std::vector<int> { a, copy, c });
            expect (board.getPedal (b) == nullptr);

            // A new pedal takes the lowest free ID.
            expectEquals (board.addPedal ("RKGate"), b);
        }

        beginTest ("An Assign slot switches a pedal on and off, both ways");
        {
            BoardProcessor board;
            board.clearBoard();
            expectEquals (board.getParameters().size(), 2 + BoardProcessor::kNumAssigns + 1, "In, Out, Assign 1-32, Scene");

            const auto chorus = board.addPedal ("RKChorus");
            const auto delay = board.addPedal ("RKDelay");
            board.movePedal (delay, Section::pre, 0);

            board.assign (2, delay, "bypass");
            expect (board.isSwitchAssignment (2));
            expectEquals (board.describeAssignment (2), juce::String ("Delay: On/Off"));
            auto& slot = board.getAssignParam (2);
            expectEquals (slot.get(), 1.0f, "at 100% while the pedal is on");

            slot.setValueNotifyingHost (0.0f);
            expect (board.isPedalBypassed (delay));
            expect (! board.isPedalBypassed (chorus));
            slot.setValueNotifyingHost (1.0f);
            expect (! board.isPedalBypassed (delay));

            // The footswitch on the tile, and the pedal's own switch, move the slot.
            board.setPedalBypassed (delay, true);
            expectEquals (slot.get(), 0.0f);
            board.getPedal (delay)->getBypassParameter()->setValueNotifyingHost (0.0f);
            expect (! board.isPedalBypassed (delay));
            expectEquals (slot.get(), 1.0f);

            // Inverted: on at 0%.
            board.setAssignmentRange (2, 0.0f, 1.0f, true);
            expectEquals (slot.get(), 0.0f);
            slot.setValueNotifyingHost (1.0f);
            expect (board.isPedalBypassed (delay));
            board.setAssignmentRange (2, 0.0f, 1.0f, false);

            // A scene switching the pedal moves the slot too.
            board.setPedalBypassed (delay, false);
            board.setScene (1);
            board.setPedalBypassed (delay, true);
            board.setScene (0);
            expectEquals (slot.get(), 1.0f);
            board.setScene (1);
            expectEquals (slot.get(), 0.0f);
            board.setScene (0);

            // A trails pedal switches on and off through its slot as well.
            const auto verb = board.addPedal ("RKVerb");
            board.setTrails (verb, true);
            board.assign (3, verb, "bypass");
            board.getAssignParam (3).setValueNotifyingHost (0.0f);
            expect (board.isPedalBypassed (verb));
            expect (board.getPedal (verb)->getBypassParameter()->getValue() < 0.5f, "rings out");
            board.getAssignParam (3).setValueNotifyingHost (1.0f);
            expect (! board.isPedalBypassed (verb));

            // Stored with the board.
            const auto json = stateJson (board);
            BoardProcessor copy;
            copy.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
            expect (copy.isSwitchAssignment (2) && copy.isSwitchAssignment (3));
            expectEquals (stateJson (copy), json);
        }

        beginTest ("Host state round-trips exactly");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto chorus = board.addPedal ("RKChorus");
            board.addPedal ("RKDistort");
            const auto verb = board.addPedal ("RKVerb");
            board.movePedal (verb, Section::pre, 0);
            board.getPedal (chorus)->findParam ("depth")->setValueNotifyingHost (0.3f);
            board.setPedalBypassed (verb, true);
            board.getInputParam().setValueNotifyingHost (board.getInputParam().convertTo0to1 (-6.0f));

            const auto json = stateJson (board);

            BoardProcessor copy;
            copy.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
            expectEquals (stateJson (copy), json);
            expect (copy.getChain() == board.getChain());
            expect (copy.isPedalBypassed (verb));
        }

        beginTest ("Board preset files round-trip, with only non-default values");
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKBoardTests");
            dir.deleteRecursively();
            const auto file = dir.getChildFile ("Test Board.json");

            BoardProcessor board;
            board.clearBoard();
            const auto chorus = board.addPedal ("RKChorus");
            board.addPedal ("RKDelay");
            board.getPedal (chorus)->findParam ("depth")->setValueNotifyingHost (0.3f);
            expect (board.isPresetModified());

            expect (board.saveBoardPreset (file).ok);
            expect (! board.isPresetModified());
            expectEquals (board.getPresetName(), juce::String ("Test Board"));

            juce::var saved;
            juce::JSON::parse (file.loadFileAsString(), saved);
            const auto* pedals = saved["pedals"].getArray();
            expect (pedals != nullptr && pedals->size() == 2);
            expectEquals ((*pedals)[0]["preset"]["plugin"].toString(), juce::String ("RKChorus"));
            expectEquals ((*pedals)[0]["preset"]["params"].getDynamicObject()->getProperties().size(), 1);
            expectEquals ((*pedals)[1]["preset"]["params"].getDynamicObject()->getProperties().size(), 0);

            BoardProcessor loaded;
            const auto r = loaded.loadBoardPreset (file);
            expect (r.ok, r.message);
            expect (loaded.getChain() == board.getChain());
            expectEquals (loaded.getPedal (chorus)->findParam ("depth")->getValue(),
                          board.getPedal (chorus)->findParam ("depth")->getValue());
            expect (! loaded.isPresetModified());

            dir.deleteRecursively();
        }

        beginTest ("The editor switches between pedals' controls, again and again");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto a = board.addPedal ("RKChorus");
            const auto b = board.addPedal ("RKDelay");

            {
                rkboard::BoardEditor editor (board);
                expectEquals (editor.getShownPedal(), a);
                for (int i = 0; i < 3; ++i)
                {
                    editor.showPedal (b);
                    expectEquals (editor.getShownPedal(), b);
                    editor.showPedal (a);
                    expectEquals (editor.getShownPedal(), a);
                }

                // Removing the shown pedal closes its controls.
                board.removePedal (a);
                expectEquals (editor.getShownPedal(), 0);
                editor.showPedal (b);
            }

            // Closing the board's window lets the pedals make new editors later.
            rkboard::BoardEditor reopened (board);
            expectEquals (reopened.getShownPedal(), b);
        }

        beginTest ("Pedals move between sections; the split takes part only with a pedal on A or B");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto a = board.addPedal ("RKChorus");
            const auto b = board.addPedal ("RKDelay", Section::post);
            expect (! board.isSplitActive());

            board.movePedal (a, Section::pathB, 0);
            expect (board.isSplitActive());
            expect (board.getSection (Section::pathB) == std::vector<int> { a });
            expect (board.sectionOf (a) == Section::pathB);
            expect (board.getChain() == std::vector<int> { a, b });

            const auto copy = board.duplicatePedal (a);
            expect (board.getSection (Section::pathB) == std::vector<int> { a, copy });

            board.movePedal (a, Section::pre, 0);
            board.movePedal (copy, Section::post, 0);
            expect (! board.isSplitActive());
            expect (board.getSection (Section::post) == std::vector<int> { copy, b });
        }

        beginTest ("Full split: both paths add up (an empty path passes the signal)");
        {
            BoardProcessor board;
            board.clearBoard();
            board.setPedalBypassed (board.addPedal ("RKUtility", Section::pathA), true);
            prepare (board);
            run (board, 50);
            const auto [in, out] = run (board, 1);

            float worst = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    worst = juce::jmax (worst, std::abs (out.getSample (ch, i) - 2.0f * in.getSample (ch, i)));
            expect (worst < 1.0e-5f, "error " + juce::String (worst));

            // Invert B: A - B = silence.
            auto& invert = board.getRouting().param ("invert_b");
            invert.setValueNotifyingHost (1.0f);
            run (board, 50);
            const auto [in2, out2] = run (board, 1);
            expect (out2.getMagnitude (0, kBlock) < 1.0e-5f);
        }

        beginTest ("Stereo split pans the paths left and right, and gives the input back");
        {
            BoardProcessor board;
            board.clearBoard();
            board.setPedalBypassed (board.addPedal ("RKUtility", Section::pathA), true);
            board.setPedalBypassed (board.addPedal ("RKUtility", Section::pathB), true);

            auto& split = board.getRouting().param ("split");
            split.setValueNotifyingHost (split.convertTo0to1 (2.0f));   // stereo
            expectEquals (board.getRouting().value ("pan_a"), -100.0f);
            expectEquals (board.getRouting().value ("pan_b"), 100.0f);

            prepare (board);
            run (board, 50);
            const auto [in, out] = run (board, 1);

            float worst = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    worst = juce::jmax (worst, std::abs (out.getSample (ch, i) - in.getSample (ch, i)));
            expect (worst < 1.0e-6f, "error " + juce::String (worst));

            // Back to full: the pans return to the centre.
            split.setValueNotifyingHost (split.convertTo0to1 (0.0f));
            expectEquals (board.getRouting().value ("pan_a"), 0.0f);
            expectEquals (board.getRouting().value ("pan_b"), 0.0f);
        }

        beginTest ("Frequency split adds back at the same level");
        {
            BoardProcessor board;
            board.clearBoard();
            board.setPedalBypassed (board.addPedal ("RKUtility", Section::pathA), true);
            board.setPedalBypassed (board.addPedal ("RKUtility", Section::pathB), true);
            auto& split = board.getRouting().param ("split");
            split.setValueNotifyingHost (split.convertTo0to1 (1.0f));
            prepare (board);
            run (board, 50);

            juce::Random random (7);
            juce::AudioBuffer<float> buffer (2, kBlock);
            juce::MidiBuffer midi;
            double in2 = 0.0, out2 = 0.0;
            for (int b = 0; b < 400; ++b)
            {
                fillNoise (buffer, random);
                in2 += buffer.getRMSLevel (0, 0, kBlock) * buffer.getRMSLevel (0, 0, kBlock);
                board.processBlock (buffer, midi);
                out2 += buffer.getRMSLevel (0, 0, kBlock) * buffer.getRMSLevel (0, 0, kBlock);
            }
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (std::sqrt (out2 / in2)), 0.0, 0.2);
        }

        beginTest ("The faster path is delayed to line up with the slower one");
        {
            BoardProcessor board;
            board.clearBoard();
            // A bypassed Saturate still delays its signal by its oversampling latency.
            const auto saturate = board.addPedal ("RKSaturate", Section::pathA);
            board.setPedalBypassed (saturate, true);
            board.setPedalBypassed (board.addPedal ("RKUtility", Section::pathB), true);
            prepare (board);

            const auto latency = board.getPedal (saturate)->getLatencySamples();
            expect (latency > 0);
            run (board, 50);
            const auto [in, out] = run (board, 1);

            float worst = 0.0f;
            for (int i = latency; i < kBlock; ++i)
                worst = juce::jmax (worst, std::abs (out.getSample (0, i) - 2.0f * in.getSample (0, i - latency)));
            expect (worst < 1.0e-5f, "error " + juce::String (worst));
        }

        beginTest ("Sections and routing round-trip through a preset, only non-default routing written");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto a = board.addPedal ("RKChorus", Section::pathA);
            const auto b = board.addPedal ("RKVerb", Section::pathB);
            const auto c = board.addPedal ("RKCompress", Section::pre);
            auto& crossover = board.getRouting().param ("crossover");
            crossover.setValueNotifyingHost (crossover.convertTo0to1 (1200.0f));

            const auto saved = board.toVar (false);
            expectEquals (saved["splits"].size(), 1);
            expectEquals (saved["splits"][0].getDynamicObject()->getProperties().size(), 1);

            BoardProcessor loaded;
            expect (loaded.fromVar (saved, false).ok);
            expect (loaded.getSection (Section::pathA) == std::vector<int> { a });
            expect (loaded.getSection (Section::pathB) == std::vector<int> { b });
            expect (loaded.getSection (Section::pre) == std::vector<int> { c });
            expectWithinAbsoluteError (loaded.getRouting().value ("crossover"), 1200.0f, 0.5f);

            // Phase 1 presets (no sections, no routing) load as a serial chain.
            const auto old = R"({ "plugin": "RKBoard", "version": 1, "pedals": [
                                    { "id": 1, "preset": { "plugin": "RKChorus", "version": 1, "params": {} } } ] })";
            expect (loaded.fromVar (juce::JSON::parse (old), false).ok);
            expect (loaded.getSection (Section::pre) == std::vector<int> { 1 });
            expectWithinAbsoluteError (loaded.getRouting().value ("crossover"), 500.0f, 0.5f);

            // Boards from before several splits: "routing" and pre / a / b / post.
            const auto oneSplit = R"({ "plugin": "RKBoard", "version": 1, "routing": { "crossover": 900 }, "pedals": [
                                         { "id": 1, "section": "a", "preset": { "plugin": "RKChorus", "version": 1, "params": {} } },
                                         { "id": 2, "section": "post", "preset": { "plugin": "RKVerb", "version": 1, "params": {} } } ] })";
            expect (loaded.fromVar (juce::JSON::parse (oneSplit), false).ok);
            expectEquals (loaded.getNumSplits(), 1);
            expect (loaded.getSection (Section::pathA) == std::vector<int> { 1 });
            expect (loaded.getSection (Section::post) == std::vector<int> { 2 });
            expectWithinAbsoluteError (loaded.getRouting().value ("crossover"), 900.0f, 0.5f);
        }

        beginTest ("Several splits: insert, process in series, remove, round-trip");
        {
            BoardProcessor board;
            board.clearBoard();
            prepare (board);
            const auto drive = board.addPedal ("RKUtility", Section::pre);
            const auto chorus = board.addPedal ("RKUtility", Section::pre);
            const auto verb = board.addPedal ("RKUtility", Section::post);

            // A split after the first pedal: chain 0 = [drive], then split 1,
            // chain 1 = [chorus], then the old split (now 2), chain 2 = [verb].
            expectEquals (board.insertSplit (Section::pre, 1), 0);
            expectEquals (board.getNumSplits(), 2);
            expect (board.getSection (chainSection (0)) == std::vector<int> { drive });
            expect (board.getSection (chainSection (1)) == std::vector<int> { chorus });
            expect (board.getSection (chainSection (2)) == std::vector<int> { verb });

            // The old split's settings moved with it, and so did what was assigned to them.
            auto& levelB = board.getRouting (1).param ("level_b");
            board.assign (0, BoardProcessor::routingOwner (1), "level_b");
            expectEquals (board.describeAssignment (0), juce::String ("Split 2: Level B"));
            board.insertSplit (Section::pre, 0);       // another one at the very start
            expectEquals (board.getNumSplits(), 3);
            expect (&board.getRouting (2).param ("level_b") == &levelB, "settings move with their split");
            expectEquals (board.getAssignment (0).pedal, BoardProcessor::routingOwner (2));

            // Pedals on the paths of different splits: audio passes, each split works.
            board.movePedal (chorus, pathSection (1, 0), 0);
            board.addPedal ("RKUtility", pathSection (2, 1));
            expect (board.isSplitActive (1) && board.isSplitActive (2) && ! board.isSplitActive (0));
            const auto [in, out] = run (board, 8);
            expect (allFinite (out));
            expect (out.getMagnitude (0, kBlock) > 0.1f, "signal through every section");

            // The merge level is a scene knob like any other.
            expect (board.setSceneControlled (BoardProcessor::routingOwner (2), "level_a", true));
            auto& levelA = board.getRouting (2).param ("level_a");
            levelA.setValueNotifyingHost (levelA.convertTo0to1 (-12.0f));
            board.setScene (1);
            levelA.setValueNotifyingHost (levelA.convertTo0to1 (0.0f));
            board.setScene (0);
            run (board, 40);
            expectWithinAbsoluteError (board.getRouting (2).value ("level_a"), -12.0f, 0.2f, "recalled in scene 1");

            // Round trip.
            const auto json = stateJson (board);
            BoardProcessor copy;
            copy.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
            expectEquals (copy.getNumSplits(), 3);
            expect (copy.getSection (pathSection (1, 0)) == std::vector<int> { chorus });
            expectEquals (copy.getAssignment (0).pedal, BoardProcessor::routingOwner (2));
            expect (copy.isSceneControlled (BoardProcessor::routingOwner (2), "level_a"));
            expectEquals (stateJson (copy), json);

            // Removing split 2: its paths join the chain, A before B, then the chain after it.
            board.removeSplit (1);
            expectEquals (board.getNumSplits(), 2);
            expect (board.getSection (chainSection (1)) == std::vector<int> { drive, chorus },
                    "chain 1, then split 2's path A (its path B and the chain after it were empty)");
            expectEquals (board.getAssignment (0).pedal, BoardProcessor::routingOwner (1), "the next split moved down");

            // The Assign view shows a split's assignment like a knob's.
            {
                rkboard::AssignPanel panel (board);
                panel.setSize (800, 500);
                panel.refresh();
            }

            // At most four.
            board.insertSplit (Section::pre, 0);
            board.insertSplit (Section::pre, 0);
            expectEquals (board.insertSplit (Section::pre, 0), -1);
            expectEquals (board.getNumSplits(), 4);
        }

        beginTest ("Assign parameters drive a knob over its range, both ways");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto delay = board.addPedal ("RKDelay");
            auto* time = board.getPedal (delay)->findParam ("time");
            time->setValueNotifyingHost (0.3f);

            board.assign (4, delay, "time");
            auto& assign = board.getAssignParam (4);
            expectWithinAbsoluteError (time->getValue(), 0.3f, 1.0e-6f, "assigning keeps the knob");
            expectWithinAbsoluteError (assign.get(), 0.3f, 1.0e-6f, "Assign 5 shows the knob");
            expectEquals (board.findAssignment (delay, "time"), 4);
            expect (board.describeAssignment (4).startsWith ("Delay: "), board.describeAssignment (4));

            board.setAssignmentRange (4, 0.2f, 0.6f, false);
            assign.setValueNotifyingHost (0.0f);
            expectWithinAbsoluteError (time->getValue(), 0.2f, 1.0e-6f);
            assign.setValueNotifyingHost (1.0f);
            expectWithinAbsoluteError (time->getValue(), 0.6f, 1.0e-6f);
            assign.setValueNotifyingHost (0.5f);
            expectWithinAbsoluteError (time->getValue(), 0.4f, 1.0e-6f);

            board.setAssignmentRange (4, 0.2f, 0.6f, true);   // heel = max
            assign.setValueNotifyingHost (0.0f);
            expectWithinAbsoluteError (time->getValue(), 0.6f, 1.0e-6f);

            // Turning the knob itself moves the Assign parameter (clamped to the range).
            time->setValueNotifyingHost (0.5f);
            expectWithinAbsoluteError (assign.get(), 0.25f, 1.0e-6f);
            time->setValueNotifyingHost (0.9f);
            expectWithinAbsoluteError (assign.get(), 0.0f, 1.0e-6f);
        }

        beginTest ("Assignments follow their pedal and go with it");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto chorus = board.addPedal ("RKChorus");
            const auto delay = board.addPedal ("RKDelay");
            board.assign (0, delay, "time");
            board.movePedal (delay, Section::pathA, 0);
            expectEquals (board.getAssignment (0).pedal, delay);

            // One slot per knob: assigning it again moves it.
            board.assign (7, delay, "time");
            expectEquals (board.getAssignment (0).pedal, 0);
            expectEquals (board.getAssignment (7).pedal, delay);

            board.assign (1, chorus, "depth");
            board.removePedal (delay);
            expectEquals (board.getAssignment (7).pedal, 0);
            expectEquals (board.getAssignment (1).pedal, chorus);

            // A new pedal taking the old ID doesn't inherit anything.
            expectEquals (board.addPedal ("RKVerb"), delay);
            expectEquals (board.findAssignment (delay, "time"), -1);
        }

        beginTest ("Assignments round-trip, ranges in real units");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto delay = board.addPedal ("RKDelay");
            auto* time = board.getPedal (delay)->findParam ("time");
            board.assign (2, delay, "time");
            board.setAssignmentRange (2, time->convertTo0to1 (100.0f), time->convertTo0to1 (800.0f), true);

            const auto saved = board.toVar (false);
            const auto& entry = (*saved["assign"].getArray())[0];
            expectEquals ((int) entry["slot"], 3);
            expectEquals (entry["param"].toString(), juce::String ("time"));
            expectWithinAbsoluteError ((double) entry["min"], 100.0, 0.5);
            expectWithinAbsoluteError ((double) entry["max"], 800.0, 0.5);

            BoardProcessor loaded;
            expect (loaded.fromVar (saved, false).ok);
            const auto info = loaded.getAssignment (2);
            expectEquals (info.pedal, delay);
            expect (info.invert);
            expectWithinAbsoluteError (info.min, time->convertTo0to1 (100.0f), 1.0e-4f);

            const auto json = stateJson (board);
            BoardProcessor copy;
            copy.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
            expectEquals (stateJson (copy), json);
        }

        beginTest ("Scenes store bypasses and scene-controlled knobs; other knobs are shared");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto drive = board.addPedal ("RKDistort");
            const auto delay = board.addPedal ("RKDelay");
            prepare (board);
            auto* time = board.getPedal (delay)->findParam ("time");
            auto* mix = board.getPedal (delay)->findParam ("mix");

            expect (board.setSceneControlled (delay, "time", true));
            time->setValueNotifyingHost (0.2f);           // scene 1
            board.setPedalBypassed (drive, true);         // scene 1

            board.setScene (1);
            expectEquals (board.getScene(), 1);
            expect (! board.isPedalBypassed (drive), "scene 2 kept the drive on");
            time->setValueNotifyingHost (0.7f);           // scene 2
            mix->setValueNotifyingHost (0.9f);            // not scene-controlled: all scenes

            board.setScene (0);
            expect (board.isPedalBypassed (drive));
            expectWithinAbsoluteError (board.getSceneValue (0, delay, "time"), 0.2f, 1.0e-6f);
            expectWithinAbsoluteError (board.getSceneValue (1, delay, "time"), 0.7f, 1.0e-6f);

            // The knob glides there (100 ms for the whole range).
            run (board, 2);
            expect (time->getValue() > 0.25f, "still gliding");
            run (board, 40);
            expectWithinAbsoluteError (time->getValue(), 0.2f, 1.0e-6f);
            expectWithinAbsoluteError (mix->getValue(), 0.9f, 1.0e-6f);
            expectWithinAbsoluteError (board.getSceneValue (1, delay, "time"), 0.7f, 1.0e-6f, "the glide isn't an edit");

            // Through the host parameter too.
            board.getSceneParam().setValueNotifyingHost (board.getSceneParam().convertTo0to1 (2.0f));
            expectEquals (board.getScene(), 1);
            expect (! board.isPedalBypassed (drive));

            board.copyScene (1, 5);
            expect (! board.getSceneBypass (5, drive));
            expectWithinAbsoluteError (board.getSceneValue (5, delay, "time"), 0.7f, 1.0e-6f);
        }

        beginTest ("Scenes round-trip");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto drive = board.addPedal ("RKDistort");
            const auto delay = board.addPedal ("RKDelay");
            board.setSceneControlled (delay, "time", true);
            board.setScene (3);
            board.setPedalBypassed (drive, true);
            board.getPedal (delay)->findParam ("time")->setValueNotifyingHost (0.6f);

            BoardProcessor loaded;
            expect (loaded.fromVar (board.toVar (false), false).ok);
            expectEquals (loaded.getScene(), 3);
            expect (loaded.getSceneBypass (3, drive));
            expect (! loaded.getSceneBypass (0, drive));
            expect (loaded.isSceneControlled (delay, "time"));
            expectWithinAbsoluteError (loaded.getSceneValue (3, delay, "time"), 0.6f, 1.0e-4f);

            const auto json = stateJson (board);
            BoardProcessor copy;
            copy.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
            expectEquals (stateJson (copy), json);
        }

        beginTest ("Trails: a bypassed delay stops taking input but its echoes ring out");
        {
            const auto measureTail = [this] (bool withTrails)
            {
                BoardProcessor board;
                board.clearBoard();
                const auto delay = board.addPedal ("RKDelay");
                expect (board.canHaveTrails (delay));
                board.setTrails (delay, withTrails);
                // One echo (375 ms) and no repeats, so the tail is over within the measuring time.
                board.getPedal (delay)->findParam ("feedback")->setValueNotifyingHost (0.0f);
                prepare (board);

                // A burst into the delay, then bypass, then silence.
                juce::AudioBuffer<float> buffer (2, kBlock);
                juce::MidiBuffer midi;
                juce::Random random (3);
                for (int b = 0; b < 20; ++b)
                {
                    fillNoise (buffer, random);
                    board.processBlock (buffer, midi);
                }
                board.setPedalBypassed (delay, true);
                expect (board.isPedalBypassed (delay));

                float tail = 0.0f, leak = 0.0f;
                for (int b = 0; b < 400; ++b)
                {
                    buffer.clear();
                    board.processBlock (buffer, midi);
                    tail = juce::jmax (tail, buffer.getMagnitude (0, kBlock));
                }

                // New input doesn't reach the delay while bypassed: after the
                // tail, a burst passes dry only (no echoes later).
                for (int b = 0; b < 4; ++b)
                {
                    fillNoise (buffer, random);
                    board.processBlock (buffer, midi);
                }
                for (int b = 0; b < 200; ++b)
                {
                    buffer.clear();
                    board.processBlock (buffer, midi);
                    leak = juce::jmax (leak, buffer.getMagnitude (0, kBlock));
                }
                return std::pair { tail, leak };
            };

            const auto [trailsTail, trailsLeak] = measureTail (true);
            const auto [cutTail, cutLeak] = measureTail (false);
            expect (trailsTail > 0.01f, "echoes ring out: " + juce::String (trailsTail));
            expect (cutTail < 1.0e-4f, "cut without trails: " + juce::String (cutTail));
            expect (trailsLeak < 1.0e-4f, "no new echoes while bypassed: " + juce::String (trailsLeak));
        }

        beginTest ("Trails: switching it doesn't change the bypass; the pedal's own switch bypasses with trails");
        {
            BoardProcessor board;
            board.clearBoard();
            const auto verb = board.addPedal ("RKVerb");
            const auto chorus = board.addPedal ("RKChorus");
            expect (! board.canHaveTrails (chorus));

            board.setPedalBypassed (verb, true);
            board.setTrails (verb, true);
            expect (board.isPedalBypassed (verb));
            expect (board.getPedal (verb)->getBypassParameter()->getValue() < 0.5f, "the pedal itself keeps running");

            board.setTrails (verb, false);
            expect (board.isPedalBypassed (verb));
            expect (board.getPedal (verb)->getBypassParameter()->getValue() >= 0.5f);

            board.setPedalBypassed (verb, false);
            board.setTrails (verb, true);
            board.getPedal (verb)->getBypassParameter()->setValueNotifyingHost (1.0f);   // its editor's switch
            expect (board.isPedalBypassed (verb));
            expect (board.getPedal (verb)->getBypassParameter()->getValue() < 0.5f);
        }

        beginTest ("Every pedal's controls are tagged with real parameters (right-click assign)");
        {
            for (const auto& type : rkboard::pedalTypes())
            {
                auto pedal = type.create();
                std::unique_ptr<juce::AudioProcessorEditor> editor (pedal->createEditorAndMakeActive());
                int tagged = 0;
                std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                {
                    if (const auto id = rkfx::findTaggedParameter (&c, c.getParentComponent()); id.isNotEmpty())
                    {
                        ++tagged;
                        expect (pedal->findParam (id) != nullptr, type.name + ": " + id);
                    }
                    for (auto* child : c.getChildren())
                        walk (*child);
                };
                walk (*editor);
                expect (tagged >= 3, type.name + " has tagged controls: " + juce::String (tagged));
                pedal->editorBeingDeleted (editor.get());
            }
        }

        beginTest ("A footswitch on an Assign slot runs RKLoop, also with a quick click");
        {
            BoardProcessor board;
            board.clearBoard();
            prepare (board);
            const auto id = board.addPedal ("RKLoop");
            auto* loop = dynamic_cast<RKLoopProcessor*> (board.getPedal (id));
            expect (loop != nullptr);
            board.assign (0, id, "record");

            // Pressed and released before the next block: still one press.
            auto& slot = board.getAssignParam (0);
            slot.setValueNotifyingHost (1.0f);
            slot.setValueNotifyingHost (0.0f);
            run (board, 4);
            expect (loop->getEngine().getState() == LoopEngine::State::recording, "recording");

            slot.setValueNotifyingHost (1.0f);
            run (board, 2);
            slot.setValueNotifyingHost (0.0f);
            run (board, 2);
            expect (loop->getEngine().getState() == LoopEngine::State::playing, "playing");
            expectEquals (loop->getEngine().getLength(), 4 * kBlock);
        }

        beginTest ("Volume pedal: taper and minimum level");
        {
            VolumeEngine::Settings s;
            s.position = 0.0f;
            expectEquals (VolumeEngine::gainFor (s), 0.0f);
            s.position = 1.0f;
            expectEquals (VolumeEngine::gainFor (s), 1.0f);
            s.position = 0.5f;
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (VolumeEngine::gainFor (s)), -18.06f, 0.05f);
            s.audioTaper = false;
            expectWithinAbsoluteError (VolumeEngine::gainFor (s), 0.5f, 1.0e-6f);
            s.position = 0.0f;
            s.minLevelDb = -30.0f;
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (VolumeEngine::gainFor (s)), -30.0f, 0.01f);
        }

        beginTest ("Wah: the heel favours the low frequency, the toe the high one");
        {
            const auto gainAt = [] (float position, float hz, WahEngine::Character character)
            {
                WahEngine::Settings s;
                s.position = position;
                s.character = character;
                WahEngine wah;
                wah.prepare (kRate, 2);
                wah.reset (s);

                juce::AudioBuffer<float> buffer (2, 4800);
                double in2 = 0.0, out2 = 0.0;
                for (int block = 0; block < 10; ++block)
                {
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        for (int ch = 0; ch < 2; ++ch)
                            buffer.setSample (ch, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (block * 4800 + i) / kRate));
                    const auto inLevel = buffer.getRMSLevel (0, 0, buffer.getNumSamples());
                    wah.process (buffer, s);
                    if (block >= 5)
                    {
                        in2 += inLevel * inLevel;
                        out2 += buffer.getRMSLevel (0, 0, buffer.getNumSamples()) * buffer.getRMSLevel (0, 0, buffer.getNumSamples());
                    }
                }
                return std::sqrt (out2 / in2);
            };

            for (const auto character : { WahEngine::Character::classic, WahEngine::Character::vocal })
            {
                expect (gainAt (0.0f, 400.0f, character) > 3.0 * gainAt (0.0f, 2200.0f, character), "heel");
                expect (gainAt (1.0f, 2200.0f, character) > 3.0 * gainAt (1.0f, 400.0f, character), "toe");
            }
            // At the peak, the band-pass passes about unity.
            expectWithinAbsoluteError (gainAt (0.0f, 400.0f, WahEngine::Character::classic), 1.0, 0.1);
            // Bass keeps the lows.
            expect (gainAt (1.0f, 60.0f, WahEngine::Character::bass) > 0.7, "bass lows");
        }

        beginTest ("Wah auto engage: on when the pedal moves, off after resting at the heel");
        {
            WahEngine::Settings s;
            s.autoEngage = true;
            s.position = 0.0f;
            WahEngine wah;
            wah.prepare (kRate, 2);
            wah.reset (s);
            expect (! wah.isEngaged());

            juce::AudioBuffer<float> buffer (2, kBlock);
            juce::Random random (11);
            fillNoise (buffer, random);
            juce::AudioBuffer<float> dry;
            dry.makeCopyOf (buffer);
            wah.process (buffer, s);
            expect (identical (buffer, dry), "dry while disengaged");

            s.position = 0.4f;
            wah.process (buffer, s);
            expect (wah.isEngaged());

            s.position = 0.0f;
            for (int b = 0; b < (int) (0.4 * kRate) / kBlock; ++b)
                wah.process (buffer, s);
            expect (wah.isEngaged(), "still on after 0.4 s at the heel");
            for (int b = 0; b < (int) (0.2 * kRate) / kBlock; ++b)
                wah.process (buffer, s);
            expect (! wah.isEngaged(), "off after half a second");
        }

        beginTest ("Factory board presets all load cleanly");
        {
            BoardProcessor board;
            const auto dir = juce::File (RKFX_SOURCE_DIR).getChildFile ("plugins/RKBoard/presets");
            expectEquals ((int) board.getFactoryPresets().size(), dir.findChildFiles (juce::File::findFiles, false, "*.json").size());
            expect (board.getFactoryPresets().size() >= 6);

            for (int i = 0; i < (int) board.getFactoryPresets().size(); ++i)
            {
                const auto& preset = board.getFactoryPresets()[(size_t) i];
                const auto r = board.loadFactoryPreset (i);
                expect (r.ok && r.message.isEmpty(), preset.name + ": " + r.message);
                expect (! board.getChain().empty(), preset.name + " has pedals");
                expectEquals (board.getPresetName(), preset.name);
                expect (! board.isPresetModified());
            }
        }

        beginTest ("Tap tempo and MIDI clock");
        {
            rkboard::TapTempo tap;
            expect (! tap.tap (10.0).has_value());
            expectWithinAbsoluteError (*tap.tap (10.5), 120.0, 0.01);
            tap.tap (11.0);
            expectWithinAbsoluteError (*tap.tap (11.5), 120.0, 0.01);
            expect (! tap.tap (20.0).has_value(), "a long pause starts over");
            expectWithinAbsoluteError (*tap.tap (20.4), 150.0, 0.01);

            rkboard::MidiClockFollower clock;
            const auto interval = 60.0 / (140.0 * 24.0);
            for (int i = 0; i < 30; ++i)
                clock.tick (i * interval);
            expectWithinAbsoluteError (*clock.bpm (29 * interval), 140.0, 0.01);
            expect (! clock.bpm (29 * interval + 1.0).has_value(), "clock stopped");

            // Resumed after a pause: the old ticks don't drag the new tempo down.
            const auto resume = 29 * interval + 2.0;
            for (int i = 0; i < 24; ++i)
                clock.tick (resume + i * interval);
            expect (! clock.bpm (resume + 23 * interval).has_value(), "starts over after a pause");
            clock.tick (resume + 24 * interval);
            expectWithinAbsoluteError (*clock.bpm (resume + 24 * interval), 140.0, 0.01);
        }

        beginTest ("Plugin: follows MIDI clock on its input, else the host's tempo");
        {
            struct HostPlayHead : juce::AudioPlayHead
            {
                juce::Optional<PositionInfo> getPosition() const override
                {
                    PositionInfo info;
                    info.setBpm (90.0);
                    info.setPpqPosition (1000.0);
                    info.setIsPlaying (true);
                    return info;
                }
            } host;

            BoardProcessor board;   // not the app: a plugin in a host
            expect (! board.isStandalone());
            board.clearBoard();
            const auto delay = board.addPedal ("RKDelay");
            board.setPlayHead (&host);
            prepare (board);

            juce::AudioBuffer<float> buffer (2, kBlock);
            double next = 0.0;
            const auto samplesPerTick = kRate * 60.0 / (100.0 * 24.0);
            const auto block = [&] (bool ticks, int startAt = -1)
            {
                juce::MidiBuffer midi;
                if (startAt >= 0)
                {
                    midi.addEvent (juce::MidiMessage::midiStart(), startAt);
                    next = startAt; // the downbeat tick with the Start
                }
                while (ticks && next < kBlock)
                {
                    midi.addEvent (juce::MidiMessage::midiClock(), (int) next);
                    next += samplesPerTick;
                }
                next = ticks ? next - kBlock : 0.0;
                buffer.clear();
                board.processBlock (buffer, midi);
            };
            const auto pedalPosition = [&] { return *board.getPedal (delay)->getPlayHead()->getPosition(); };

            // Not following: clock is ignored, the pedals get the host's play head.
            for (int b = 0; b < 50; ++b)
                block (true);
            expect (! board.isClockSettingTempo());
            expectWithinAbsoluteError (board.getPedalTempo(), 90.0, 1.0e-9);
            expectWithinAbsoluteError (*pedalPosition().getBpm(), 90.0, 1.0e-9);

            // Following: the clock's tempo, once a beat of it has arrived.
            board.setFollowClock (true);
            for (int b = 0; b < 200; ++b)   // a beat of clock (24 ticks) is 113 blocks
                block (true);
            expect (board.isClockSettingTempo());
            expectWithinAbsoluteError (board.getPedalTempo(), 100.0, 0.5);
            expectWithinAbsoluteError (*pedalPosition().getBpm(), 100.0, 0.5);

            // A Start puts beat 0 on its sample (the next block starts after it).
            block (true, 100);
            block (true);
            expectWithinAbsoluteError (*pedalPosition().getPpqPosition(), (2 * kBlock - 100) * 100.0 / (60.0 * kRate), 0.01);

            // The clock stops: back to the host's tempo.
            for (int b = 0; b < 100; ++b)
                block (false);
            expect (! board.isClockSettingTempo());
            expectWithinAbsoluteError (board.getPedalTempo(), 90.0, 1.0e-9);
            expectWithinAbsoluteError (*pedalPosition().getBpm(), 90.0, 1.0e-9);

            // Saved with the host session, not in board presets.
            juce::MemoryBlock state;
            board.getStateInformation (state);
            BoardProcessor restored;
            restored.setStateInformation (state.getData(), (int) state.getSize());
            expect (restored.isFollowingClock(), "restored from the session");
            expect (! board.toVar (false).hasProperty ("followClock"), "not in board presets");

            board.setFollowClock (false);
            board.getStateInformation (state);
            restored.setStateInformation (state.getData(), (int) state.getSize());
            expect (! restored.isFollowingClock());
            board.setPlayHead (nullptr);
        }

        beginTest ("Tuner pitch detection and note names");
        {
            rkboard::PitchDetector detector;
            detector.prepare (kRate, 4096);
            std::vector<float> window (4096);
            for (const auto hz : { 41.2, 82.41, 110.0, 196.0, 440.0, 1046.5 })
            {
                for (int i = 0; i < 4096; ++i)
                    window[(size_t) i] = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / kRate);
                const auto found = detector.detect (window.data());
                expectWithinAbsoluteError (1200.0f * std::log2 (found / (float) hz), 0.0f, 1.0f, juce::String (hz) + " Hz");
            }

            std::fill (window.begin(), window.end(), 0.0f);
            expectEquals (detector.detect (window.data()), 0.0f);

            const auto a = rkboard::PitchDetector::noteFor (440.0f);
            expect (a.name == "A" && a.octave == 4);
            expectWithinAbsoluteError (a.cents, 0.0f, 0.01f);
            const auto e = rkboard::PitchDetector::noteFor (82.41f);
            expect (e.name == "E" && e.octave == 2, e.name + juce::String (e.octave));
            expectWithinAbsoluteError (rkboard::PitchDetector::noteFor (445.0f).cents, 19.56f, 0.05f);
            expectWithinAbsoluteError (rkboard::PitchDetector::noteFor (442.0f, 442.0f).cents, 0.0f, 0.01f);
        }

        beginTest ("MIDI map: one controller per target, saved and loaded");
        {
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKBoardTests/midi-map.json");
            file.deleteFile();
            {
                rkboard::MidiMap map (file);
                map.set ({ rkboard::LearnTarget::firstAssign, 1, 11, false });
                map.set ({ rkboard::LearnTarget::firstBypass + 2, 2, 64, true });
                map.set ({ rkboard::LearnTarget::firstAssign + 1, 1, 11, false });   // takes CC 11 over
                expect (! map.find (rkboard::LearnTarget::firstAssign).has_value());
                expectEquals (map.lookup (1, 11), rkboard::LearnTarget::firstAssign + 1);
                expectEquals (map.lookup (2, 64), rkboard::LearnTarget::firstBypass + 2 + 1000);
                expectEquals (map.lookup (3, 64), -1);
            }
            rkboard::MidiMap loaded (file);
            expectEquals ((int) loaded.getMappings().size(), 2);
            expect (loaded.find (rkboard::LearnTarget::firstBypass + 2)->momentary);
            file.getParentDirectory().deleteRecursively();
        }

        beginTest ("Standalone: MIDI learn, expression pedal, footswitches, scenes, tuner, clock");
        {
            const auto rig = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RKBoardTests/rig");
            rig.deleteRecursively();

            BoardProcessor board;
            board.setStandaloneForTesting (rig);
            board.clearBoard();
            const auto delay = board.addPedal ("RKDelay");
            const auto drive = board.addPedal ("RKDistort");
            board.assign (0, delay, "mix");
            prepare (board);

            juce::AudioBuffer<float> buffer (2, kBlock);
            const auto send = [&] (std::initializer_list<juce::MidiMessage> messages, int blocks = 1)
            {
                for (int b = 0; b < blocks; ++b)
                {
                    juce::MidiBuffer midi;
                    if (b == 0)
                        for (const auto& m : messages)
                            midi.addEvent (m, 0);
                    buffer.clear();
                    board.processBlock (buffer, midi);
                }
            };

            // Learn Assign 1 from CC 11.
            board.startLearning (rkboard::LearnTarget::firstAssign);
            send ({ juce::MidiMessage::controllerEvent (1, 11, 0) });
            board.flushPendingUpdate();
            expectEquals (board.getLearningTarget(), -1);
            expectEquals (board.getMidiMap()->find (rkboard::LearnTarget::firstAssign)->cc, 11);
            expect (rig.getChildFile ("midi-map.json").existsAsFile(), "saved");

            // The expression pedal glides the knob (no 7-bit steps).
            send ({ juce::MidiMessage::controllerEvent (1, 11, 127) }, 1);
            const auto midway = board.getAssignParam (0).get();
            expect (midway > 0.0f && midway < 1.0f, "gliding: " + juce::String (midway));
            send ({}, 40);
            expectWithinAbsoluteError (board.getAssignParam (0).get(), 1.0f, 1.0e-3f);
            expectWithinAbsoluteError (board.getPedal (delay)->findParam ("mix")->getValue(), 1.0f, 1.0e-3f);

            // A toggling footswitch, then a momentary one.
            board.getMidiMap()->set ({ rkboard::LearnTarget::firstBypass + drive - 1, 1, 20, false });
            send ({ juce::MidiMessage::controllerEvent (1, 20, 127) });
            expect (board.isPedalBypassed (drive));
            send ({ juce::MidiMessage::controllerEvent (1, 20, 0) });
            expect (board.isPedalBypassed (drive), "release does nothing");
            send ({ juce::MidiMessage::controllerEvent (1, 20, 127) });
            expect (! board.isPedalBypassed (drive));

            board.getMidiMap()->set ({ rkboard::LearnTarget::firstBypass + drive - 1, 1, 20, true });
            send ({ juce::MidiMessage::controllerEvent (1, 20, 0) });
            expect (board.isPedalBypassed (drive), "momentary: off when released");
            send ({ juce::MidiMessage::controllerEvent (1, 20, 127) });
            expect (! board.isPedalBypassed (drive), "on while held");

            // Scenes: a CC value picks one, program changes too.
            board.getMidiMap()->set ({ rkboard::LearnTarget::scene, 1, 30, false });
            send ({ juce::MidiMessage::controllerEvent (1, 30, 3) });
            expectEquals (board.getScene(), 3);
            send ({ juce::MidiMessage::programChange (1, 7) });
            expectEquals (board.getScene(), 7);

            // The tuner mutes the output (after a short fade) while it is on.
            board.getMidiMap()->set ({ rkboard::LearnTarget::tuner, 1, 40, false });
            send ({ juce::MidiMessage::controllerEvent (1, 40, 127) });
            expect (board.isTunerActive());
            {
                juce::Random random (5);
                juce::MidiBuffer midi;
                for (int b = 0; b < 20; ++b)
                {
                    fillNoise (buffer, random);
                    board.processBlock (buffer, midi);
                }
                expect (buffer.getMagnitude (0, kBlock) < 1.0e-6f, "muted while tuning");
                std::vector<float> copy (1024);
                board.copyTunerInput (copy.data(), 1024);
                expect (juce::FloatVectorOperations::findMaximum (copy.data(), 1024) > 0.1f, "the tuner gets the input");
            }
            send ({ juce::MidiMessage::controllerEvent (1, 40, 0) });
            send ({ juce::MidiMessage::controllerEvent (1, 40, 127) });
            expect (! board.isTunerActive());

            // MIDI clock sets the tempo while following it.
            board.setFollowClock (true);
            const auto samplesPerTick = kRate * 60.0 / (100.0 * 24.0);
            double next = 0.0;
            for (int b = 0; b < 200; ++b)
            {
                juce::MidiBuffer midi;
                while (next < kBlock)
                {
                    midi.addEvent (juce::MidiMessage::midiClock(), (int) next);
                    next += samplesPerTick;
                }
                next -= kBlock;
                buffer.clear();
                board.processBlock (buffer, midi);
            }
            expectWithinAbsoluteError (board.getTempo(), 100.0, 0.5);
            board.setFollowClock (false);

            board.setTempo (93.0);
            BoardProcessor loaded;
            loaded.fromVar (board.toVar (false), false);
            expectWithinAbsoluteError (loaded.getTempo(), 93.0, 0.01);

            rig.getParentDirectory().deleteRecursively();
        }

        beginTest ("Unknown pedals in a preset are skipped with a note");
        {
            const auto json = R"({ "plugin": "RKBoard", "version": 1, "pedals": [
                                     { "id": 2, "preset": { "plugin": "RKWarpDrive", "version": 1, "params": {} } },
                                     { "id": 5, "preset": { "plugin": "RKChorus", "version": 1, "params": { "depth": 40 } } } ] })";
            BoardProcessor board;
            const auto r = board.fromVar (juce::JSON::parse (json), false);
            expect (r.ok);
            expect (r.message.contains ("RKWarpDrive"), r.message);
            expect (board.getChain() == std::vector<int> { 5 });
            expectWithinAbsoluteError (rkfx::ProcessorBase::valueOf (board.getPedal (5)->findParam ("depth")), 40.0f, 0.01f);

            expect (! board.fromVar (juce::JSON::parse (R"({ "plugin": "RKChorus", "params": {} })"), false).ok);
        }
    }
};

static BoardTests boardTests;

int main (int, char**)
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i))
            failures += r->failures;

    if (failures > 0)
    {
        std::fprintf (stderr, "RKBoardTests: %d test failure(s)\n", failures);
        return 1;
    }

    std::printf ("RKBoardTests: all tests passed\n");
    return 0;
}
