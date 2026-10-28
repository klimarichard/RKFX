// RKBoard: the board processor with real pedals (every RKFX plugin built in).

#include "BoardEditor.h"
#include "pedals/VolumeEngine.h"
#include "pedals/WahEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

using rkboard::BoardProcessor;
using rkboard::Section;

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
                const auto id = board.addPedal (type.name);
                expect (id == 1, type.name);

                auto* pedal = board.getPedal (id);
                expect (pedal != nullptr && pedal->getConfig().name == type.name, type.name);

                const auto [in, out] = run (board, 40);
                expect (allFinite (out), type.name + " output is finite");

                // Plugins keep theirs in plugins/<Name>/presets, board-only pedals in
                // plugins/RKBoard/pedals/presets/<Name>.
                const auto source = juce::File (RKFX_SOURCE_DIR).getChildFile ("plugins");
                auto presetDir = source.getChildFile (type.name + "/presets");
                if (! presetDir.isDirectory())
                    presetDir = source.getChildFile ("RKBoard/pedals/presets/" + type.name);
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

        beginTest ("Board is full at 12 pedals");
        {
            BoardProcessor board;
            board.clearBoard();
            for (int i = 0; i < 12; ++i)
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

        beginTest ("Bypass parameters follow the pedal ID both ways");
        {
            BoardProcessor board;
            board.clearBoard();
            board.addPedal ("RKChorus");
            const auto delay = board.addPedal ("RKDelay");
            board.movePedal (delay, Section::pre, 0);

            auto* hostBypass = board.getParameters()[delay - 1];
            hostBypass->setValueNotifyingHost (1.0f);
            expect (board.isPedalBypassed (delay));
            expect (! board.isPedalBypassed (1));

            // The pedal's own switch (its editor) shows on the host parameter.
            board.getPedal (delay)->getBypassParameter()->setValueNotifyingHost (0.0f);
            expectEquals (hostBypass->getValue(), 0.0f);

            board.setPedalBypassed (1, true);
            expectEquals (board.getParameters()[0]->getValue(), 1.0f);
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
            expectEquals (saved["routing"].getDynamicObject()->getProperties().size(), 1);

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
            expectEquals (board.getParameters()[verb - 1]->getValue(), 1.0f);
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
