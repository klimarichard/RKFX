#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <optional>
#include <vector>

namespace rkboard
{

/** What a MIDI controller can be learned to (standalone only). */
namespace LearnTarget
{
    constexpr int firstAssign = 0;       // + slot (0..31): continuous
    constexpr int firstBypass = 100;     // + pedal ID - 1 (0..11): switch
    constexpr int scene = 200;           // the CC value picks the scene (0..15)
    constexpr int tap = 201;             // switch: tap tempo
    constexpr int tuner = 202;           // switch: tuner on / off

    juce::String key (int target);                 // "assign3", "bypass5", "scene", "tap", "tuner"
    int fromKey (const juce::String&);             // -1 when unknown
    juce::String describe (int target);            // "Assign 3", "Pedal 5 Bypass", ...
    bool isSwitch (int target);
}

struct MidiMapping
{
    int target = -1;
    int channel = 1;          // 1..16
    int cc = 0;               // 0..127
    bool momentary = false;   // switches: on while held (else each press toggles)
};

/** The rig's MIDI mappings, kept in midi-map.json (they belong to the rig,
    not to a sound, so they stay the same across board presets).

    Edited on the message thread; the audio thread looks controllers up in a
    lock-free table. A controller drives one target, and a target has one
    controller. */
class MidiMap
{
public:
    explicit MidiMap (juce::File file = defaultFile());

    static juce::File defaultFile();     // ~/Library/Application Support/RKFX/RKBoard/midi-map.json

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
    std::vector<MidiMapping> mappings;
    std::array<std::atomic<int>, 16 * 128> table;

    void rebuildTable();
};

/** Standalone options of the rig (settings.json next to midi-map.json). */
struct RigSettings
{
    bool followClock = false;
    float tunerReference = 440.0f;      // 430..450 Hz
    bool tunerMutes = true;

    static juce::File defaultFile();
    void load (const juce::File& = defaultFile());
    bool save (const juce::File& = defaultFile()) const;
};

} // namespace rkboard
