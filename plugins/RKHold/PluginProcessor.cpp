#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKHold";
        c.stateVersion = 1;
        c.hasMix = false;                     // Hold Level and Dry Level are the mix
        c.monoToStereo = true;                // layers spread across the stereo field
        c.accent = juce::Colour (0xff2e9c7a); // Delay family, sea green
        c.family = rkfx::Palette::Family::delay;
        return c;
    }
}

RKHoldProcessor::RKHoldProcessor()
    : rkfx::ProcessorBase (makeConfig(), HoldParams::create()),
      hold (findParam ("hold")), holdMode (findParam ("hold_mode")), mode (findParam ("mode")), range (findParam ("range")),
      layers (findParam ("layers")), clear (findParam ("clear")), octave (findParam ("octave")), attack (findParam ("attack")),
      release (findParam ("release")), movement (findParam ("movement")), tone (findParam ("tone")), width (findParam ("width")),
      holdLevel (findParam ("hold_level")), dryLevel (findParam ("dry_level"))
{
    hold->addListener (&listener);
    clear->addListener (&listener);
}

RKHoldProcessor::~RKHoldProcessor()
{
    hold->removeListener (&listener);
    clear->removeListener (&listener);
}

void RKHoldProcessor::switchChanged (int parameterIndex, float value)
{
    const auto down = value >= 0.5f;
    if (parameterIndex == hold->getParameterIndex())
    {
        if (down && ! holdDown.exchange (true))
            ++holdPresses;
        else if (! down && holdDown.exchange (false))
            ++holdLifts;
    }
    else if (parameterIndex == clear->getParameterIndex())
    {
        if (down && ! clearDown.exchange (true))
            ++clearPresses;
        else if (! down)
            clearDown = false;
    }
}

HoldEngine::Settings RKHoldProcessor::currentSettings() const
{
    using Range = rkfx::PitchTracker::Range;
    constexpr Range ranges[] = { Range::violin, Range::voice, Range::guitar, Range::bass };

    HoldEngine::Settings s;
    s.latch      = valueOf (holdMode) >= 0.5f;
    s.mode       = (HoldEngine::Mode) juce::jlimit (0, 2, juce::roundToInt (valueOf (mode)));
    s.range      = ranges[juce::jlimit (0, 3, juce::roundToInt (valueOf (range)))];
    s.add        = valueOf (layers) >= 0.5f;
    s.octave     = juce::roundToInt (valueOf (octave)) - 1;
    s.attackMs   = valueOf (attack);
    s.releaseMs  = valueOf (release);
    s.movement   = valueOf (movement) / 100.0f;
    s.toneHz     = valueOf (tone);
    s.width      = valueOf (width) / 100.0f;
    s.holdGainDb = valueOf (holdLevel);
    s.dryGainDb  = valueOf (dryLevel);
    return s;
}

HoldEngine::LayerInfo RKHoldProcessor::getLayer (int index) const noexcept
{
    const auto& l = shownLayers[(size_t) index];
    return { l.active.load(), l.note.load(), l.level.load() };
}

void RKHoldProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.setSettings (currentSettings());
    engine.prepare (sampleRate);
}

void RKHoldProcessor::resetDsp()
{
    // Back from bypass (or a new start): no layers, as every RKFX plugin starts fresh.
    engine.setSettings (currentSettings());
    engine.reset();
}

void RKHoldProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.setSettings (currentSettings());

    for (auto n = holdPresses.exchange (0); n > 0; --n)
        engine.press();
    for (auto n = holdLifts.exchange (0); n > 0; --n)
        engine.lift();
    if (clearPresses.exchange (0) > 0)
        engine.releaseAll();

    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    for (int i = 0; i < HoldEngine::kLayers; ++i)
    {
        const auto l = engine.getLayer (i);
        auto& shown = shownLayers[(size_t) i];
        shown.active = l.active;
        shown.note = l.note;
        shown.level = l.level;
    }
}

juce::AudioProcessorEditor* RKHoldProcessor::createEditor()
{
    return new RKHoldEditor (*this);
}

// The plugin's entry point; RKBoard holds every plugin and has its own.
#if ! RKFX_BOARD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKHoldProcessor();
}
#endif
