#pragma once

#include <juce_core/juce_core.h>

#include "host/MidiMap.h"

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
    constexpr int firstBypass = 100;     // + pedal ID - 1: switch
    constexpr int numBypass = 64;        // = BoardProcessor::kMaxPedals
    constexpr int scene = 200;           // the CC value picks the scene (0..15)
    constexpr int tap = 201;             // switch: tap tempo
    constexpr int tuner = 202;           // switch: tuner on / off

    juce::String key (int target);                 // "assign3", "bypass5", "scene", "tap", "tuner"
    int fromKey (const juce::String&);             // -1 when unknown
    juce::String describe (int target);            // "Assign 3", "Pedal 5 On/Off", ...
    bool isSwitch (int target);
}

using rkfx::MidiMapping;

/** The rig's MIDI mappings, kept in midi-map.json: the pedal host's MidiMap
    with the board's targets. */
class MidiMap : public rkfx::MidiMap
{
public:
    explicit MidiMap (juce::File file = defaultFile());
    static juce::File defaultFile();     // ~/Library/Application Support/RKFX/RKBoard/midi-map.json
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
