#include <juce_audio_basics/juce_audio_basics.h>

#include "LoopEngine.h"

#include <cmath>

/** RKLoop DSP: record, play back exactly, overdub, undo and redo, stop and
    play from the start, clear, reverse and half speed, the seam without a
    click, the input always passing, and the loop kept across a new
    prepare at the same rate. */
class LoopTests : public juce::UnitTest
{
public:
    LoopTests() : juce::UnitTest ("RKLoop engine") {}

    void runTest() override
    {
        using State = LoopEngine::State;
        using Button = LoopEngine::Button;

        beginTest ("Record, then the loop plays back over the input");
        {
            LoopEngine e;
            e.prepare (48000.0);
            expect (e.getState() == State::empty);

            e.press (Button::record);
            auto a = sine (440.0, 0.5f, 48000);
            const auto recorded = a;
            process (e, a);
            expect (e.getState() == State::recording);
            expectEquals (e.getRecordedLength(), 48000);
            expectWithinAbsoluteError (a.getSample (0, 1000), recorded.getSample (0, 1000), 1.0e-6f, "the input passes while recording");

            e.press (Button::record);
            expect (e.getState() == State::playing);
            expectEquals (e.getLength(), 48000);

            // Silence in: the loop comes out, in the middle exactly as recorded.
            auto out = silence (48000);
            process (e, out);
            for (const auto i : { 12000, 24000, 36000 })
                expectWithinAbsoluteError (out.getSample (0, i), recorded.getSample (0, i), 1.0e-5f);
        }

        beginTest ("The seam doesn't click");
        {
            LoopEngine e;
            e.prepare (48000.0);
            e.press (Button::record);
            auto a = sine (100.0, 0.5f, 47900);   // not a whole number of cycles: a hard seam without fades
            process (e, a);
            e.press (Button::record);
            auto out = silence (96000);
            process (e, out);
            float maxStep = 0.0f;
            for (int i = 1000; i < 96000; ++i)
                maxStep = juce::jmax (maxStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            // A 100 Hz sine at 0.5 moves at most 0.0066 per sample; the faded seam no more than a little over that.
            expect (maxStep < 0.02f, "largest step " + juce::String (maxStep));
        }

        beginTest ("Overdub, undo, redo");
        {
            LoopEngine e;
            e.prepare (48000.0);
            recordLoop (e, 440.0, 24000);
            const auto base = loopOutput (e, 24000);

            e.press (Button::record);   // overdub
            expect (e.getState() == State::overdubbing);
            auto layer = sine (660.0, 0.25f, 24000);
            process (e, layer);
            e.press (Button::record);   // back to play
            expect (e.getState() == State::playing);
            expect (e.canUndo());

            const auto dubbed = loopOutput (e, 24000);
            expect (difference (base, dubbed) > 0.1f, "the overdub is in the loop");

            e.press (Button::undo);
            loopOutput (e, 24000);      // the swap runs while playing
            expect (e.canRedo());
            expect (difference (base, loopOutput (e, 24000)) < 1.0e-5f, "undo restores the loop");

            e.press (Button::undo);     // redo
            loopOutput (e, 24000);
            expect (difference (dubbed, loopOutput (e, 24000)) < 1.0e-5f, "redo brings the overdub back");
        }

        beginTest ("Stop, play from the start, clear");
        {
            LoopEngine e;
            e.prepare (48000.0);
            recordLoop (e, 440.0, 24000);
            loopOutput (e, 5000);

            e.press (Button::stop);
            auto out = silence (2400);
            process (e, out);
            expect (e.getState() == State::stopped);
            auto quiet = silence (2400);
            process (e, quiet);
            expect (quiet.getMagnitude (0, 0, 2400) < 1.0e-6f, "silent when stopped");

            e.press (Button::record);
            expect (e.getState() == State::playing);
            expectEquals (e.getPosition(), 0.0, "from the start");

            e.press (Button::clear);
            auto after = silence (4800);
            process (e, after);
            expect (e.getState() == State::empty);
            expectEquals (e.getLength(), 0);
        }

        beginTest ("Reverse and half speed");
        {
            LoopEngine e;
            e.prepare (48000.0);
            e.press (Button::record);
            auto ramp = silence (24000);
            for (int i = 0; i < 24000; ++i)
                ramp.setSample (0, i, 0.5f * (float) i / 24000.0f);
            ramp.copyFrom (1, 0, ramp, 0, 0, 24000);
            const auto recorded = ramp;
            process (e, ramp);
            e.press (Button::record);

            e.setReverse (true);
            auto out = silence (12000);
            process (e, out);
            // Playing backwards from where it was (the loop start): the end of the ramp, falling.
            expect (out.getSample (0, 6000) > out.getSample (0, 9000), "falling");

            e.setReverse (false);
            e.setHalfSpeed (true);
            const auto from = e.getPosition();
            auto slow = silence (2000);
            process (e, slow);
            expectWithinAbsoluteError (e.getPosition() - from, 1000.0, 1.0, "half the samples per sample");
            juce::ignoreUnused (recorded);
        }

        beginTest ("A new prepare at the same rate keeps the loop");
        {
            LoopEngine e;
            e.prepare (48000.0);
            recordLoop (e, 440.0, 24000);
            e.prepare (48000.0);
            expectEquals (e.getLength(), 24000);
            e.prepare (44100.0);
            expectEquals (e.getLength(), 0, "a new rate starts empty");
        }
    }

private:
    static juce::AudioBuffer<float> sine (double hz, float amplitude, int n)
    {
        juce::AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i)
        {
            const auto x = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / 48000.0);
            b.setSample (0, i, x);
            b.setSample (1, i, x);
        }
        return b;
    }

    static juce::AudioBuffer<float> silence (int n)
    {
        juce::AudioBuffer<float> b (2, n);
        b.clear();
        return b;
    }

    static void process (LoopEngine& e, juce::AudioBuffer<float>& b)
    {
        for (int start = 0; start < b.getNumSamples(); start += 64)
        {
            float* ch[] { b.getWritePointer (0, start), b.getWritePointer (1, start) };
            e.process (ch, 2, juce::jmin (64, b.getNumSamples() - start));
        }
    }

    static void recordLoop (LoopEngine& e, double hz, int n)
    {
        e.press (LoopEngine::Button::record);
        auto a = sine (hz, 0.5f, n);
        process (e, a);
        e.press (LoopEngine::Button::record);
    }

    /** What the loop plays over silence for @p n samples. */
    static juce::AudioBuffer<float> loopOutput (LoopEngine& e, int n)
    {
        auto b = silence (n);
        process (e, b);
        return b;
    }

    /** Largest difference, after the first playback's 5 ms fade-in. */
    static float difference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        float d = 0.0f;
        for (int i = 480; i < juce::jmin (a.getNumSamples(), b.getNumSamples()); ++i)
            d = juce::jmax (d, std::abs (a.getSample (0, i) - b.getSample (0, i)));
        return d;
    }
};

static LoopTests loopTests;
