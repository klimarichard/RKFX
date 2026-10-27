#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <map>
#include <vector>

namespace rkfx
{

/** What the preset format needs to know about one parameter. Values are
    always in real units (Hz, dB, ms, percent, as shown on the GUI). */
struct ParamInfo
{
    enum class Kind { Float, Int, Bool, Choice };

    juce::String id;
    Kind kind = Kind::Float;
    double minValue = 0.0, maxValue = 1.0, defaultValue = 0.0;
    juce::StringArray choiceKeys;   // Choice only: lowercase keys written to JSON
};

/** Real-unit values keyed by parameter ID. Choice = index, Bool = 0 or 1. */
using ParamValues = std::map<juce::String, double>;

/** Migrates a preset's raw "params" object (as read from JSON, before
    validation) from an older state version to the current one. */
using PresetMigration = std::function<void (juce::DynamicObject& params, int fromVersion)>;

struct PresetReadResult
{
    bool ok = false;
    juce::String error;          // why the file was refused (when ! ok)
    juce::StringArray notes;     // skipped or corrected values (shown as a short note)
    juce::String name;           // "name" key, empty when absent
    int version = 0;
    bool modified = false;       // "modified" key (host state only)
    ParamValues values;          // every parameter: missing ones hold their default
};

/** The JSON format shared by preset files and the state saved in the host:

        { "plugin": "RKChorus", "version": 1, "name": "...", "params": { ... } }

    Preset files hold only non-default values; host state holds every value
    (plus "name" and "modified"), so a gig never depends on a preset file. */
namespace PresetFormat
{
    /** The plugin, version, optional name, then params. Floats are rounded to
        6 significant digits; whole numbers are written without decimals. */
    juce::var toVar (const juce::String& pluginName, int version, const juce::String& presetName,
                     const std::vector<ParamInfo>& params, const ParamValues& values, bool onlyNonDefault);

    juce::String toJson (const juce::var& preset);

    /** Parses and validates. Refuses (ok == false) only when the text isn't a
        JSON object, belongs to another plugin, or has no "params" object.
        Everything else is lenient: unknown keys and invalid values are
        skipped with a note, out-of-range numbers are clamped with a note,
        missing values take their defaults. */
    PresetReadResult read (const juce::String& json, const juce::String& expectedPlugin,
                           int currentVersion, const std::vector<ParamInfo>& params,
                           const PresetMigration& migrate = {});

    /** Every parameter at its default. */
    ParamValues defaults (const std::vector<ParamInfo>& params);
}

} // namespace rkfx
