#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PresetFormat.h"

#include <memory>
#include <vector>

namespace rkfx
{

/** A choice parameter that also knows the lowercase keys written to presets
    (for example names "Low-pass", "High-pass" with keys "lowpass", "highpass"). */
class KeyedChoiceParameter : public juce::AudioParameterChoice
{
public:
    KeyedChoiceParameter (const juce::ParameterID& id, const juce::String& name,
                          const juce::StringArray& displayNames, juce::StringArray presetKeys, int defaultIndex);

    const juce::StringArray keys;
};

/** Builds a plugin's parameter list with the suite's shared units, scaling and
    text formatting, so every plugin shows and parses values the same way:

        frequency   Hz / kHz, logarithmic
        decibels    dB
        time        ms / s, logarithmic
        percent     %

    Parameters are numbered in the order they are added (the AU/VST3 version
    hint, which also sets the order hosts list them in). IDs are permanent:
    only ever append new parameters, never remove, rename or reorder. */
class ParamBuilder
{
public:
    void addFrequency (const juce::String& id, const juce::String& name, float minHz, float maxHz, float defaultHz);
    void addDecibels  (const juce::String& id, const juce::String& name, float minDb, float maxDb, float defaultDb);
    void addTime      (const juce::String& id, const juce::String& name, float minMs, float maxMs, float defaultMs);
    void addPercent   (const juce::String& id, const juce::String& name, float minPct, float maxPct, float defaultPct);
    void addInt       (const juce::String& id, const juce::String& name, int minValue, int maxValue, int defaultValue,
                       const juce::String& unit = {});
    void addBool      (const juce::String& id, const juce::String& name, bool defaultValue);
    void addChoice    (const juce::String& id, const juce::String& name,
                       const juce::StringArray& displayNames, const juce::StringArray& presetKeys, int defaultIndex);

    /** Escape hatch for a float with a custom range or formatting. */
    void addFloat (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> range,
                   float defaultValue, std::function<juce::String (float)> toText,
                   std::function<float (const juce::String&)> fromText);

    /** The next version hint; used by the base processor to append the
        standard controls after the plugin's own parameters. */
    juce::ParameterID nextId (const juce::String& id) { return { id, nextHint++ }; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

private:
    int nextHint = 1;
};

/** Suite-wide value formatting and parsing (also used by the GUI). */
namespace Units
{
    juce::String frequency (float hz);
    juce::String decibels (float db);
    juce::String time (float ms);
    juce::String percent (float pct);

    /** Parses "1.2k", "1200 Hz", "1.2 kHz". */
    float parseFrequency (const juce::String& text);
    /** Parses "250", "250 ms", "1.5 s". */
    float parseTime (const juce::String& text);
    /** Parses the leading number, ignoring any unit. */
    float parseNumber (const juce::String& text);

    /** Exact logarithmic mapping between min and max (both > 0). */
    juce::NormalisableRange<float> logRange (float min, float max);
}

/** Describes a parameter for the preset format (real units, keys). */
ParamInfo describe (const juce::RangedAudioParameter& p);

} // namespace rkfx
