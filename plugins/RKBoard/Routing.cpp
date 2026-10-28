#include "Routing.h"

#include "plugin/ParamBuilder.h"

namespace rkboard
{

juce::String sectionKey (Section s)
{
    if (! isPath (s))
        return "chain" + juce::String (chainOf (s));
    return juce::String (pathOf (s) == 0 ? "a" : "b") + juce::String (splitOf (s) + 1);
}

Section sectionFromKey (const juce::String& key)
{
    // Boards from before several splits: one split.
    if (key == "pre")  return Section::pre;
    if (key == "a")    return Section::pathA;
    if (key == "b")    return Section::pathB;
    if (key == "post") return Section::post;

    if (key.startsWith ("chain"))
    {
        const auto k = key.substring (5).getIntValue();
        if (k >= 0 && k <= kMaxSplits)
            return chainSection (k);
    }
    else if ((key.startsWith ("a") || key.startsWith ("b")) && key.length() > 1)
    {
        const auto split = key.substring (1).getIntValue() - 1;
        if (split >= 0 && split < kMaxSplits)
            return pathSection (split, key[0] == 'a' ? 0 : 1);
    }
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
