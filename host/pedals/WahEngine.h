#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/StateVariableFilter.h"

#include <array>

/** The wah: a resonant band-pass swept by the pedal position.

    - Sweep: logarithmic between the low and high frequency, with an
      audio-taper feel (more change towards the toe) like the classic pedal.
    - Characters: classic (one resonant band-pass), vocal (two broader
      formant-like peaks, more vowel-like), bass (an octave lower, with the
      lows below 150 Hz kept).
    - Auto engage: moving the pedal switches the wah on; resting at the heel
      for half a second switches it off (dry), with a short crossfade. */
class WahEngine
{
public:
    enum class Character { classic, vocal, bass };

    struct Settings
    {
        float position = 0.5f;          // 0 heel .. 1 toe
        Character character = Character::classic;
        float lowHz = 400.0f, highHz = 2200.0f;
        float resonance = 0.6f;         // 0..1
        bool autoEngage = false;
    };

    /** The band-pass centre for a position (before the character's shift). */
    static float frequencyFor (const Settings& s, float position);

    void prepare (double sampleRate, int numChannels);
    void reset (const Settings& s);
    void process (juce::AudioBuffer<float>& buffer, const Settings& s);

    bool isEngaged() const noexcept { return engaged; }

private:
    struct Channel
    {
        rkfx::StateVariableFilter peak, formant, lows;
    };

    double sampleRate = 48000.0;
    std::array<Channel, 2> channels;
    float smoothedPosition = 0.5f, positionCoeff = 0.0f;
    float engageAmount = 1.0f, engageStep = 0.0f;
    bool engaged = true;
    float restPosition = 0.0f;
    int heelSamples = 0;

    void updateEngage (const Settings& s, int numSamples);
};
