#include "MidiMap.h"

#include "plugin/PresetLibrary.h"

namespace rkfx
{

//==============================================================================
MidiMap::MidiMap (juce::File f, Targets t) : file (std::move (f)), targets (std::move (t))
{
    for (auto& entry : table)
        entry = -1;
    load();
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
                m.target = targets.fromKey (entry["target"].toString());
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
        entry->setProperty ("target", targets.key (m.target));
        entry->setProperty ("channel", m.channel);
        entry->setProperty ("cc", m.cc);
        if (targets.isSwitch (m.target))
            entry->setProperty ("mode", m.momentary ? "momentary" : "toggle");
        list.add (juce::var (entry));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("mappings", list);
    return file.getParentDirectory().createDirectory()
        && rkfx::PresetLibrary::writeAtomically (file, juce::JSON::toString (juce::var (root)));
}

} // namespace rkfx
