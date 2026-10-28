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
MidiMap::MidiMap (juce::File f) : file (std::move (f))
{
    for (auto& entry : table)
        entry = -1;
    load();
}

juce::File MidiMap::defaultFile()
{
    return rigFolder().getChildFile ("midi-map.json");
}

std::optional<MidiMapping> MidiMap::find (int target) const
{
    for (const auto& m : mappings)
        if (m.target == target)
            return m;
    return std::nullopt;
}

void MidiMap::set (const MidiMapping& mapping)
{
    // One controller per target and one target per controller.
    mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&] (const MidiMapping& m)
                    {
                        return m.target == mapping.target || (m.channel == mapping.channel && m.cc == mapping.cc);
                    }),
                    mappings.end());
    mappings.push_back (mapping);
    rebuildTable();
    save();
}

void MidiMap::clear (int target)
{
    mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [=] (const MidiMapping& m) { return m.target == target; }),
                    mappings.end());
    rebuildTable();
    save();
}

void MidiMap::rebuildTable()
{
    for (auto& entry : table)
        entry = -1;
    for (const auto& m : mappings)
        if (m.channel >= 1 && m.channel <= 16 && m.cc >= 0 && m.cc < 128)
            table[(size_t) ((m.channel - 1) * 128 + m.cc)] = m.target + (m.momentary ? 1000 : 0);
}

void MidiMap::load()
{
    mappings.clear();

    juce::var root;
    if (file.existsAsFile() && juce::JSON::parse (file.loadFileAsString(), root).wasOk())
        if (const auto* list = root["mappings"].getArray())
            for (const auto& entry : *list)
            {
                MidiMapping m;
                m.target = LearnTarget::fromKey (entry["target"].toString());
                m.channel = juce::jlimit (1, 16, (int) entry["channel"]);
                m.cc = juce::jlimit (0, 127, (int) entry["cc"]);
                m.momentary = entry["mode"].toString() == "momentary";
                if (m.target >= 0)
                    mappings.push_back (m);
            }

    rebuildTable();
}

bool MidiMap::save() const
{
    juce::Array<juce::var> list;
    for (const auto& m : mappings)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("target", LearnTarget::key (m.target));
        entry->setProperty ("channel", m.channel);
        entry->setProperty ("cc", m.cc);
        if (LearnTarget::isSwitch (m.target))
            entry->setProperty ("mode", m.momentary ? "momentary" : "toggle");
        list.add (juce::var (entry));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("mappings", list);
    return file.getParentDirectory().createDirectory()
        && rkfx::PresetLibrary::writeAtomically (file, juce::JSON::toString (juce::var (root)));
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
