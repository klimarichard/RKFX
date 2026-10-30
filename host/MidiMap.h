#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <functional>
#include <optional>
#include <vector>

namespace rkfx
{

struct MidiMapping
{
    int target = -1;
    int channel = 1;          // 1..16
    int cc = 0;               // 0..127
    bool momentary = false;   // switches: on while held (else each press toggles)
};

/** A standalone app's MIDI mappings (RKBoard, RKMix): controllers learned to
    targets, kept in a JSON file of the rig (they belong to the rig, not to a
    sound, so they stay the same across presets). What the targets are is the
    app's: it names them for the file and says which are switches.

    Edited on the message thread; the audio thread looks controllers up in a
    lock-free table. A controller drives one target, and a target has one
    controller. */
class MidiMap
{
public:
    struct Targets
    {
        std::function<juce::String (int)> key;                // for the file; empty: unknown
        std::function<int (const juce::String&)> fromKey;     // -1: unknown
        std::function<bool (int)> isSwitch;
    };

    MidiMap (juce::File, Targets);
    virtual ~MidiMap() = default;

    const std::vector<MidiMapping>& getMappings() const noexcept { return mappings; }
    std::optional<MidiMapping> find (int target) const;

    /** Adds or replaces (and saves). */
    void set (const MidiMapping&);
    void clear (int target);

    /** Audio thread: the mapping for a controller, encoded as target + 1000 *
        momentary, or -1. */
    int lookup (int channel, int cc) const noexcept
    {
        return channel >= 1 && channel <= 16 && cc >= 0 && cc < 128 ? table[(size_t) ((channel - 1) * 128 + cc)].load() : -1;
    }

    void load();
    bool save() const;

private:
    juce::File file;
    Targets targets;
    std::vector<MidiMapping> mappings;
    std::array<std::atomic<int>, 16 * 128> table;

    void rebuildTable();
};

} // namespace rkfx
