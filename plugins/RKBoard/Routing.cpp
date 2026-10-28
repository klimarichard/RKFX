#include "Routing.h"

#include "plugin/ParamBuilder.h"

namespace rkboard
{

juce::String sectionKey (Section s)
{
    switch (s)
    {
        case Section::pre:   return "pre";
        case Section::pathA: return "a";
        case Section::pathB: return "b";
        case Section::post:  return "post";
    }
    return "pre";
}

Section sectionFromKey (const juce::String& key)
{
    for (const auto s : { Section::pre, Section::pathA, Section::pathB, Section::post })
        if (sectionKey (s) == key)
            return s;
    return Section::pre;
}

//==============================================================================
RoutingSettings::RoutingSettings()
{
    rkfx::ParamBuilder p;
    p.addChoice    ("split", "Split", { "Full", "Frequency", "Stereo" }, { "full", "frequency", "stereo" }, 0);
    p.addFrequency ("crossover", "Crossover", 50.0f, 5000.0f, 500.0f);
    p.addDecibels  ("level_a", "Level A", -24.0f, 12.0f, 0.0f);
    p.addPercent   ("pan_a", "Pan A", -100.0f, 100.0f, 0.0f);
    p.addDecibels  ("level_b", "Level B", -24.0f, 12.0f, 0.0f);
    p.addPercent   ("pan_b", "Pan B", -100.0f, 100.0f, 0.0f);
    p.addBool      ("invert_b", "Invert B", false);

    for (auto& owned : p.params)
    {
        infos.push_back (rkfx::describe (*owned));
        addParameter (owned.release());
    }
}

juce::RangedAudioParameter& RoutingSettings::param (const juce::String& id) const
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() == id)
            return *ranged;

    jassertfalse;
    return *dynamic_cast<juce::RangedAudioParameter*> (getParameters().getFirst());
}

float RoutingSettings::value (const juce::String& id) const
{
    const auto& p = param (id);
    return p.convertFrom0to1 (p.getValue());
}

SplitMode RoutingSettings::getSplitMode() const
{
    return (SplitMode) juce::jlimit (0, 2, juce::roundToInt (value ("split")));
}

juce::var RoutingSettings::toVar (bool onlyNonDefault) const
{
    rkfx::ParamValues values;
    for (const auto& info : infos)
        values[info.id] = value (info.id);

    return rkfx::PresetFormat::toVar ("RKBoard", 1, {}, infos, values, onlyNonDefault)["params"];
}

juce::StringArray RoutingSettings::fromVar (const juce::var& params)
{
    // Wrapped as a preset so the shared lenient reader validates it.
    auto* wrapper = new juce::DynamicObject();
    wrapper->setProperty ("plugin", "RKBoard");
    wrapper->setProperty ("version", 1);
    wrapper->setProperty ("params", params.isObject() ? params : juce::var (new juce::DynamicObject()));

    const auto r = rkfx::PresetFormat::read (rkfx::PresetFormat::toJson (juce::var (wrapper)), "RKBoard", 1, infos);

    for (const auto& [id, v] : r.values)
    {
        auto& p = param (id);
        p.setValueNotifyingHost (p.convertTo0to1 ((float) v));
    }
    return r.notes;
}

} // namespace rkboard
