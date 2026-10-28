// RKBoard: the board processor with real pedals (every RKFX plugin built in).

#include "BoardProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

using rkboard::BoardProcessor;

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
                if (a.getSample (ch, i) != b.getSample (ch, i))
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

                const auto presetDir = juce::File (RKFX_SOURCE_DIR).getChildFile ("plugins/" + type.name + "/presets");
                const auto files = presetDir.findChildFiles (juce::File::findFiles, false, "*.json");
                expectEquals ((int) pedal->getFactoryPresets().size(), files.size(), type.name + " factory presets");
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

            board.movePedal (c, 0);
            expect (board.getChain() == std::vector<int> { c, a, b });
            board.movePedal (c, 3);   // to the end
            expect (board.getChain() == std::vector<int> { a, b, c });
            board.movePedal (a, 2);   // between b and c
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
            board.movePedal (delay, 0);

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
            board.movePedal (verb, 0);
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
