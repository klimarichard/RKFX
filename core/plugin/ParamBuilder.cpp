#include "ParamBuilder.h"

#include <cmath>

namespace rkfx
{

KeyedChoiceParameter::KeyedChoiceParameter (const juce::ParameterID& id, const juce::String& paramName,
                                            const juce::StringArray& displayNames, juce::StringArray presetKeys,
                                            int defaultIndex)
    : juce::AudioParameterChoice (id, paramName, displayNames, defaultIndex),
      keys (std::move (presetKeys))
{
    jassert (keys.size() == displayNames.size());
}

//==============================================================================
namespace Units
{
    juce::String frequency (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String (hz, hz >= 100.0f ? 0 : (hz >= 10.0f ? 1 : 2)) + " Hz";
    }

    juce::String decibels (float db)
    {
        if (db <= -100.0f)
            return "-inf dB";
        const auto rounded = std::abs (db) < 0.05f ? 0.0f : db;
        return (rounded > 0.0f ? "+" : "") + juce::String (rounded, 1) + " dB";
    }

    juce::String time (float ms)
    {
        if (ms >= 1000.0f)
            return juce::String (ms / 1000.0f, 2) + " s";
        return juce::String (ms, ms >= 100.0f ? 0 : (ms >= 10.0f ? 1 : 2)) + " ms";
    }

    juce::String degrees (float deg)
    {
        return juce::String (juce::roundToInt (deg)) + juce::String::fromUTF8 ("\xc2\xb0");
    }

    juce::String percent (float pct)
    {
        return juce::String (juce::roundToInt (pct)) + " %";
    }

    float parseNumber (const juce::String& text)
    {
        return text.trim().retainCharacters ("0123456789.-+eE").getFloatValue();
    }

    float parseFrequency (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        const auto value = t.initialSectionContainingOnly ("0123456789.-+").getFloatValue();
        return t.containsChar ('k') ? value * 1000.0f : value;
    }

    float parseTime (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        const auto value = t.initialSectionContainingOnly ("0123456789.-+").getFloatValue();
        const auto unit = t.substring (t.initialSectionContainingOnly ("0123456789.-+").length()).trim();
        return unit.startsWith ("s") ? value * 1000.0f : value;
    }

    juce::NormalisableRange<float> logRange (float min, float max)
    {
        jassert (min > 0.0f && max > min);
        const auto ratio = std::log (max / min);

        return { min, max,
                 [ratio] (float start, float, float x)  { return start * std::exp (ratio * x); },
                 [ratio] (float start, float, float v)  { return std::log (v / start) / ratio; },
                 [] (float start, float end, float v)   { return juce::jlimit (start, end, v); } };
    }
}

//==============================================================================
void ParamBuilder::addFloat (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> range,
                             float defaultValue, std::function<juce::String (float)> toText,
                             std::function<float (const juce::String&)> fromText)
{
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withStringFromValueFunction ([toText] (float v, int) { return toText (v); })
                          .withValueFromStringFunction ([fromText] (const juce::String& s) { return fromText (s); });

    params.push_back (std::make_unique<juce::AudioParameterFloat> (nextId (id), name, range, defaultValue, attributes));
}

void ParamBuilder::addFrequency (const juce::String& id, const juce::String& name, float minHz, float maxHz, float defaultHz)
{
    addFloat (id, name, Units::logRange (minHz, maxHz), defaultHz, Units::frequency, Units::parseFrequency);
}

void ParamBuilder::addDecibels (const juce::String& id, const juce::String& name, float minDb, float maxDb, float defaultDb)
{
    addFloat (id, name, { minDb, maxDb }, defaultDb, Units::decibels, Units::parseNumber);
}

void ParamBuilder::addTime (const juce::String& id, const juce::String& name, float minMs, float maxMs, float defaultMs)
{
    if (minMs > 0.0f)
        addFloat (id, name, Units::logRange (minMs, maxMs), defaultMs, Units::time, Units::parseTime);
    else
        addFloat (id, name, { minMs, maxMs }, defaultMs, Units::time, Units::parseTime);
}

void ParamBuilder::addPercent (const juce::String& id, const juce::String& name, float minPct, float maxPct, float defaultPct)
{
    addFloat (id, name, { minPct, maxPct }, defaultPct, Units::percent, Units::parseNumber);
}

void ParamBuilder::addDegrees (const juce::String& id, const juce::String& name, float minDeg, float maxDeg, float defaultDeg)
{
    addFloat (id, name, { minDeg, maxDeg }, defaultDeg, Units::degrees, Units::parseNumber);
}

void ParamBuilder::addInt (const juce::String& id, const juce::String& name, int minValue, int maxValue, int defaultValue,
                           const juce::String& unit)
{
    auto attributes = juce::AudioParameterIntAttributes()
                          .withStringFromValueFunction ([unit] (int v, int) { return unit.isEmpty() ? juce::String (v)
                                                                                                    : juce::String (v) + " " + unit; })
                          .withValueFromStringFunction ([] (const juce::String& s) { return juce::roundToInt (Units::parseNumber (s)); });

    params.push_back (std::make_unique<juce::AudioParameterInt> (nextId (id), name, minValue, maxValue, defaultValue, attributes));
}

void ParamBuilder::addBool (const juce::String& id, const juce::String& name, bool defaultValue)
{
    params.push_back (std::make_unique<juce::AudioParameterBool> (nextId (id), name, defaultValue));
}

void ParamBuilder::addChoice (const juce::String& id, const juce::String& name,
                              const juce::StringArray& displayNames, const juce::StringArray& presetKeys, int defaultIndex)
{
    params.push_back (std::make_unique<KeyedChoiceParameter> (nextId (id), name, displayNames, presetKeys, defaultIndex));
}

//==============================================================================
ParamInfo describe (const juce::RangedAudioParameter& p)
{
    ParamInfo info;
    info.id = p.getParameterID();

    const auto& range = p.getNormalisableRange();
    info.minValue = range.start;
    info.maxValue = range.end;
    info.defaultValue = p.convertFrom0to1 (p.getDefaultValue());

    if (auto* choice = dynamic_cast<const KeyedChoiceParameter*> (&p))
    {
        info.kind = ParamInfo::Kind::Choice;
        info.choiceKeys = choice->keys;
    }
    else if (auto* plainChoice = dynamic_cast<const juce::AudioParameterChoice*> (&p))
    {
        info.kind = ParamInfo::Kind::Choice;
        for (const auto& c : plainChoice->choices)
            info.choiceKeys.add (c.toLowerCase().removeCharacters (" -/"));
    }
    else if (dynamic_cast<const juce::AudioParameterBool*> (&p) != nullptr)
    {
        info.kind = ParamInfo::Kind::Bool;
    }
    else if (dynamic_cast<const juce::AudioParameterInt*> (&p) != nullptr)
    {
        info.kind = ParamInfo::Kind::Int;
    }

    return info;
}

} // namespace rkfx
