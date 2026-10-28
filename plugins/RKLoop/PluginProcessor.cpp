#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKLoop";
        c.stateVersion = 1;
        c.hasMix = false;                     // the input always passes; Loop Level sets the loop
        c.accent = juce::Colour (0xff58c48a); // Delay family, mint green
        c.family = rkfx::Palette::Family::delay;
        return c;
    }
}

RKLoopProcessor::RKLoopProcessor()
    : rkfx::ProcessorBase (makeConfig(), LoopParams::create()),
      reverse (findParam ("reverse")), halfSpeed (findParam ("half_speed")), level (findParam ("level")),
      loop (findParam ("loop")), sync (findParam ("sync"))
{
    for (int i = 0; i < LoopEngine::kLoops; ++i)
        roles[(size_t) i] = findParam (LoopParams::roleId (i));

    for (int b = 0; b < kButtons; ++b)
    {
        buttons[(size_t) b] = findParam (LoopParams::buttonIds[b]);
        buttons[(size_t) b]->addListener (&buttonListener);
    }
}

RKLoopProcessor::~RKLoopProcessor()
{
    for (auto* b : buttons)
        b->removeListener (&buttonListener);
}

void RKLoopProcessor::buttonChanged (int parameterIndex, float newValue)
{
    for (int b = 0; b < kButtons; ++b)
        if (buttons[(size_t) b]->getParameterIndex() == parameterIndex)
        {
            const auto isDown = newValue >= 0.5f;
            if (isDown && ! down[(size_t) b].exchange (true))
                ++presses[(size_t) b];
            else if (! isDown)
                down[(size_t) b] = false;
        }
}

void RKLoopProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.prepare (sampleRate);
}

void RKLoopProcessor::resetDsp()
{
    engine.reset();
}

void RKLoopProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    for (int i = 0; i < LoopEngine::kLoops; ++i)
        engine.setRole (i, valueOf (roles[(size_t) i]) >= 0.5f ? LoopEngine::Role::layer : LoopEngine::Role::part);
    engine.setSync (valueOf (sync) >= 0.5f);

    // The Loop parameter selects (a host widget), before the footswitches act.
    if (const auto wanted = juce::roundToInt (valueOf (loop)); wanted != shownLoop)
    {
        shownLoop = wanted;
        engine.select (wanted - 1);
    }

    for (int b = 0; b < kButtons; ++b)
        for (auto n = presses[(size_t) b].exchange (0); n > 0; --n)
            engine.press ((LoopEngine::Button) b);

    // Next / Previous moved the selection: the parameter follows.
    if (const auto selected = engine.getSelected() + 1; selected != shownLoop)
    {
        shownLoop = selected;
        loop->setValueNotifyingHost (loop->convertTo0to1 ((float) selected));
    }

    engine.setReverse (valueOf (reverse) >= 0.5f);
    engine.setHalfSpeed (valueOf (halfSpeed) >= 0.5f);
    engine.setLevel (valueOf (level) / 100.0f);
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RKLoopProcessor::createEditor()
{
    return new RKLoopEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKLoopProcessor();
}
#endif
