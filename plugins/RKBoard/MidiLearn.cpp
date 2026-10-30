#include "MidiLearn.h"

#include "plugin/PresetLibrary.h"

namespace rkboard
{

namespace
{
    juce::File rigFolder()
    {
        return rkfx::PresetLibrary::defaultSettingsFile().getParentDirectory().getChildFile ("RKBoard");
    }
}

//==============================================================================
juce::String LearnTarget::key (int target)
{
    if (target >= firstAssign && target < firstAssign + 32)  return "assign" + juce::String (target - firstAssign + 1);
    if (target >= firstBypass && target < firstBypass + numBypass)  return "bypass" + juce::String (target - firstBypass + 1);
    if (target == scene) return "scene";
    if (target == tap)   return "tap";
    if (target == tuner) return "tuner";
    return {};
}

int LearnTarget::fromKey (const juce::String& k)
{
    if (k.startsWith ("assign"))
        if (const auto n = k.substring (6).getIntValue(); n >= 1 && n <= 32)
            return firstAssign + n - 1;
    if (k.startsWith ("bypass"))
        if (const auto n = k.substring (6).getIntValue(); n >= 1 && n <= numBypass)
            return firstBypass + n - 1;
    if (k == "scene") return scene;
    if (k == "tap")   return tap;
    if (k == "tuner") return tuner;
    return -1;
}

juce::String LearnTarget::describe (int target)
{
    if (target >= firstAssign && target < firstAssign + 32)  return "Assign " + juce::String (target - firstAssign + 1);
    if (target >= firstBypass && target < firstBypass + numBypass)  return "Pedal " + juce::String (target - firstBypass + 1) + " On/Off";
    if (target == scene) return "Scene";
    if (target == tap)   return "Tap Tempo";
    if (target == tuner) return "Tuner";
    return {};
}

bool LearnTarget::isSwitch (int target)
{
    return (target >= firstBypass && target < firstBypass + numBypass) || target == tap || target == tuner;
}

//==============================================================================
MidiMap::MidiMap (juce::File f) : rkfx::MidiMap (std::move (f), { LearnTarget::key, LearnTarget::fromKey, LearnTarget::isSwitch })
{
}

juce::File MidiMap::defaultFile()
{
    return rigFolder().getChildFile ("midi-map.json");
}

//==============================================================================
juce::File RigSettings::defaultFile()
{
    return rigFolder().getChildFile ("settings.json");
}

void RigSettings::load (const juce::File& f)
{
    juce::var root;
    if (! f.existsAsFile() || juce::JSON::parse (f.loadFileAsString(), root).failed())
        return;

    followClock = (bool) root.getProperty ("followClock", followClock);
    tunerReference = juce::jlimit (430.0f, 450.0f, (float) (double) root.getProperty ("tunerReference", tunerReference));
    tunerMutes = (bool) root.getProperty ("tunerMutes", tunerMutes);
}

bool RigSettings::save (const juce::File& f) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("followClock", followClock);
    root->setProperty ("tunerReference", (double) tunerReference);
    root->setProperty ("tunerMutes", tunerMutes);
    return f.getParentDirectory().createDirectory()
        && rkfx::PresetLibrary::writeAtomically (f, juce::JSON::toString (juce::var (root)));
}

} // namespace rkboard
