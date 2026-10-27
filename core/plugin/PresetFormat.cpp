#include "PresetFormat.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rkfx
{

namespace
{
    /** Rounds to 6 significant digits so float noise (0.6000000238) never
        reaches the file, and whole numbers become ints so they print without
        a decimal point. */
    juce::var tidyNumber (double v)
    {
        char buf[32];
        std::snprintf (buf, sizeof (buf), "%.6g", v);
        const auto rounded = std::strtod (buf, nullptr);

        if (std::abs (rounded) < 1.0e9 && rounded == std::floor (rounded))
            return (int) rounded;

        return rounded;
    }

    bool isDefault (const ParamInfo& p, double v)
    {
        if (p.kind == ParamInfo::Kind::Float)
        {
            const auto span = std::abs (p.maxValue - p.minValue);
            return std::abs (v - p.defaultValue) <= span * 1.0e-6;
        }

        return std::lround (v) == std::lround (p.defaultValue);
    }

    juce::var valueToJson (const ParamInfo& p, double v)
    {
        switch (p.kind)
        {
            case ParamInfo::Kind::Bool:   return v >= 0.5;
            case ParamInfo::Kind::Int:    return (int) std::lround (v);
            case ParamInfo::Kind::Choice:
            {
                const auto index = juce::jlimit (0, juce::jmax (0, p.choiceKeys.size() - 1), (int) std::lround (v));
                return p.choiceKeys[index];
            }
            case ParamInfo::Kind::Float:  break;
        }

        return tidyNumber (v);
    }

    /** Converts one JSON value; returns false (and fills @p why) when it can't be used. */
    bool valueFromJson (const ParamInfo& p, const juce::var& json, double& out, juce::String& why)
    {
        switch (p.kind)
        {
            case ParamInfo::Kind::Bool:
                if (json.isBool())                    { out = (bool) json ? 1.0 : 0.0; return true; }
                if (json.isInt() || json.isInt64())   { const auto i = (juce::int64) json;
                                                        if (i == 0 || i == 1) { out = (double) i; return true; } }
                why = "expected true or false";
                return false;

            case ParamInfo::Kind::Choice:
                if (json.isString())
                {
                    const auto index = p.choiceKeys.indexOf (json.toString(), true);
                    if (index >= 0) { out = index; return true; }
                    why = "unknown value \"" + json.toString() + "\"";
                    return false;
                }
                why = "expected one of: " + p.choiceKeys.joinIntoString (", ");
                return false;

            case ParamInfo::Kind::Int:
            case ParamInfo::Kind::Float:
                if (json.isInt() || json.isInt64() || json.isDouble())
                {
                    const auto v = (double) json;
                    if (! std::isfinite (v)) { why = "not a number"; return false; }
                    out = (p.kind == ParamInfo::Kind::Int) ? (double) std::llround (v) : v;
                    return true;
                }
                why = "expected a number";
                return false;
        }

        return false;
    }
}

ParamValues PresetFormat::defaults (const std::vector<ParamInfo>& params)
{
    ParamValues values;
    for (const auto& p : params)
        values[p.id] = p.defaultValue;
    return values;
}

juce::var PresetFormat::toVar (const juce::String& pluginName, int version, const juce::String& presetName,
                               const std::vector<ParamInfo>& params, const ParamValues& values, bool onlyNonDefault)
{
    auto* paramsObject = new juce::DynamicObject();

    for (const auto& p : params)
    {
        const auto it = values.find (p.id);
        const auto v = it != values.end() ? it->second : p.defaultValue;

        if (onlyNonDefault && isDefault (p, v))
            continue;

        paramsObject->setProperty (p.id, valueToJson (p, v));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("plugin", pluginName);
    root->setProperty ("version", version);
    if (presetName.isNotEmpty())
        root->setProperty ("name", presetName);
    root->setProperty ("params", juce::var (paramsObject));

    return juce::var (root);
}

juce::String PresetFormat::toJson (const juce::var& preset)
{
    return juce::JSON::toString (preset, false) + "\n";
}

PresetReadResult PresetFormat::read (const juce::String& json, const juce::String& expectedPlugin,
                                     int currentVersion, const std::vector<ParamInfo>& params,
                                     const PresetMigration& migrate)
{
    PresetReadResult result;
    result.values = defaults (params);

    juce::var root;
    if (juce::JSON::parse (json, root).failed() || root.getDynamicObject() == nullptr)
    {
        result.error = "Not a valid preset file";
        return result;
    }

    const auto plugin = root["plugin"].toString();
    if (plugin != expectedPlugin)
    {
        result.error = plugin.isEmpty() ? "Not a preset file (no plugin name)"
                                        : "This preset is for " + plugin + ", not " + expectedPlugin;
        return result;
    }

    auto* paramsObject = root["params"].getDynamicObject();
    if (paramsObject == nullptr)
    {
        result.error = "The preset has no parameter values";
        return result;
    }

    const auto& versionVar = root["version"];
    if (versionVar.isInt() || versionVar.isInt64() || versionVar.isDouble())
    {
        result.version = (int) versionVar;
    }
    else
    {
        result.version = currentVersion;
        result.notes.add ("No version; assumed current");
    }

    if (result.version > currentVersion)
        result.notes.add ("Made by a newer version; some settings may be missing");

    // Work on a copy so migration never modifies the caller's parsed tree.
    auto paramsCopy = paramsObject->clone();
    if (result.version < currentVersion && migrate)
        migrate (*paramsCopy, result.version);

    for (const auto& prop : paramsCopy->getProperties())
    {
        const auto key = prop.name.toString();
        const auto* info = [&]() -> const ParamInfo*
        {
            for (const auto& p : params)
                if (p.id == key)
                    return &p;
            return nullptr;
        }();

        if (info == nullptr)
        {
            result.notes.add ("Skipped unknown \"" + key + "\"");
            continue;
        }

        double v = 0.0;
        juce::String why;
        if (! valueFromJson (*info, prop.value, v, why))
        {
            result.notes.add ("Skipped \"" + key + "\": " + why);
            continue;
        }

        if (info->kind == ParamInfo::Kind::Float || info->kind == ParamInfo::Kind::Int)
        {
            const auto lo = juce::jmin (info->minValue, info->maxValue);
            const auto hi = juce::jmax (info->minValue, info->maxValue);
            if (v < lo || v > hi)
            {
                v = juce::jlimit (lo, hi, v);
                result.notes.add ("\"" + key + "\" was out of range, set to " + tidyNumber (v).toString());
            }
        }

        result.values[key] = v;
    }

    result.name = root["name"].toString();
    result.modified = (bool) root.getProperty ("modified", false);
    result.ok = true;
    return result;
}

} // namespace rkfx
