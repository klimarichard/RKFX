#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

/** The RKSustain MIDI processing: holds notes after the keys are released,
    for instruments without a sustain pedal input, footswitch-only rigs,
    and pads that should ring until the next chord.

    Modes:
    - Sustain: like a sustain pedal: while the pedal is down, released notes
      keep sounding; lifting it ends them (a held key keeps its note).
    - Sostenuto: only the notes held when the pedal goes down keep sounding.
    - Hold: every note keeps sounding after its key is released, until a new
      note is played after all keys were up (the next chord replaces it).
      Pressing the pedal ends them all.
    - Latch: each key press switches its note on, the next press off.

    The pedal is a MIDI controller (64 by default; swallowed, so the
    instrument doesn't sustain twice), the Pedal parameter (for a host
    footswitch), or either. A sustained note played again restarts (note-off
    then note-on). Everything else passes through. GUI-free and
    allocation-free after prepare(). */
class SustainEngine
{
public:
    enum class Mode { sustain, sostenuto, hold, latch };

    struct Settings
    {
        Mode mode = Mode::sustain;
        int controller = 64;            // the pedal's CC, -1 = none (the Pedal parameter only)
        bool pedalParameter = false;    // the Pedal parameter (a host footswitch)
    };

    void prepare();
    void reset();
    void setSettings (const Settings& s) noexcept { settings = s; }

    void process (juce::MidiBuffer&) noexcept;
    void releaseAll (juce::MidiBuffer&) noexcept;

private:
    Settings settings;
    juce::MidiBuffer out;

    std::array<std::array<bool, 128>, 16> keyDown {}, sounding {}, caught {};   // caught: held at sostenuto's press
    std::array<bool, 16> ccPedal {};
    bool pedal = false, parameterWas = false;
    int keysDown = 0;

    void setPedal (bool down, int position) noexcept;
    void endHeld (int position, bool exceptKeysDown) noexcept;
    void noteOn (int channel, int note, juce::uint8 velocity, int position) noexcept;
    void noteOff (int channel, int note, int position) noexcept;
};
