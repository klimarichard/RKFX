#include "VolumePedal.h"

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKVolume";
        c.hasMix = false;
        c.hasOutput = false;                  // the pedal is the level
        c.accent = juce::Colour (0xffb4b4b4); // Utility family, light gray
        c.family = rkfx::Palette::Family::utility;
        return c;
    }

    /** -inf (stored as -100) at the bottom of the travel, then -60 to -20 dB. */
    juce::NormalisableRange<float> minLevelRange()
    {
        constexpr float offZone = 0.05f, lo = -60.0f, hi = -20.0f;
        constexpr auto silent = VolumeEngine::kSilentDb;
        return { silent, hi,
                 [=] (float, float, float x) { return x < offZone ? silent : lo + (hi - lo) * (x - offZone) / (1.0f - offZone); },
                 [=] (float, float, float v) { return v <= -80.0f ? 0.0f : offZone + (1.0f - offZone) * (juce::jlimit (lo, hi, v) - lo) / (hi - lo); },
                 [=] (float, float, float v) { return v <= -80.0f ? silent : juce::jlimit (lo, hi, v); } };
    }

    rkfx::ParamBuilder makeParams()
    {
        rkfx::ParamBuilder p;
        p.addPercent ("position", "Position", 0.0f, 100.0f, 100.0f);
        p.addChoice  ("taper", "Taper", { "Linear", "Audio" }, { "linear", "audio" }, 1);
        p.addFloat   ("min_level", "Minimum Level", minLevelRange(), VolumeEngine::kSilentDb,
                      [] (float db) { return db <= -80.0f ? juce::String ("-inf dB") : rkfx::Units::decibels (db); },
                      [] (const juce::String& t) { return t.containsIgnoreCase ("inf") ? VolumeEngine::kSilentDb : rkfx::Units::parseNumber (t); });
        return p;
    }
}

RKVolumeProcessor::RKVolumeProcessor()
    : rkfx::ProcessorBase (makeConfig(), makeParams()),
      position (findParam ("position")), taper (findParam ("taper")), minLevel (findParam ("min_level"))
{
}

VolumeEngine::Settings RKVolumeProcessor::currentSettings() const
{
    VolumeEngine::Settings s;
    s.position = valueOf (position) / 100.0f;
    s.audioTaper = valueOf (taper) >= 0.5f;
    s.minLevelDb = valueOf (minLevel);
    return s;
}

void RKVolumeProcessor::prepareDsp (double sampleRate, int, int)
{
    engine.prepare (sampleRate);
}

void RKVolumeProcessor::resetDsp()
{
    engine.reset (currentSettings());
}

void RKVolumeProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    engine.process (buffer, currentSettings());
}

juce::AudioProcessorEditor* RKVolumeProcessor::createEditor()
{
    return new RKVolumeEditor (*this);
}

//==============================================================================
RKVolumeEditor::RKVolumeEditor (RKVolumeProcessor& p)
    : rkfx::EditorBase (p, 480, 360)
{
    position = std::make_unique<rkfx::Knob> (*p.findParam ("position"), "Position");
    minLevel = std::make_unique<rkfx::Knob> (*p.findParam ("min_level"), "Minimum");
    taper = std::make_unique<rkfx::SegmentedSwitch> (*p.findParam ("taper"), accent(), juce::StringArray { "Linear", "Audio" }, "Taper");

    for (juce::Component* c : std::initializer_list<juce::Component*> { position.get(), minLevel.get(), taper.get() })
        addToCanvas (*c);

    finishSetup();
}

void RKVolumeEditor::layoutContent()
{
    using rkfx::Knob;
    auto area = contentArea().reduced (8, 0).withTrimmedTop (30);
    auto row = area.removeFromTop (Knob::kDefaultHeight).withSizeKeepingCentre (2 * Knob::kDefaultWidth + 200, Knob::kDefaultHeight);
    position->setBounds (row.removeFromLeft (Knob::kDefaultWidth));
    row.removeFromLeft (24);
    taper->setBounds (row.removeFromLeft (152).withSizeKeepingCentre (152, 52));
    row.removeFromLeft (24);
    minLevel->setBounds (row);
}

void RKVolumeEditor::paintContent (juce::Graphics& g)
{
    drawSection (g, contentArea(), "Volume");
}
