#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MixInserts.h"
#include "MixSettings.h"

#include <array>
#include <vector>

/** RKMix's 16 scenes. A scene holds the mix's values: the faders and
    mutes, every strip and bus setting that is a value (trims, EQ, dynamics,
    pans, sends, assigns, section switches), each insert's on/off, and the
    insert knobs made scene controlled. It never holds the structure (links,
    bus types and formats, tap points, outputs, keys, which plugin is where,
    solo, talkback and oscillator setup, names, colours).

    Recalling a scene stores the current one first (so edits made while a
    scene is active go into it), then glides the continuous values over the
    glide time and switches the rest at once. A recall-safe channel or bus
    keeps its values. A scene never recalled holds what the mix had when it
    was first left. Message thread. */
class MixScenes : private juce::Timer
{
public:
    static constexpr int kNumScenes = 16;
    static constexpr int kMaxKnobs = 64;

    MixScenes (MixSettings&, juce::AudioProcessor& host, InsertRack&);
    ~MixScenes() override;

    int getCurrent() const noexcept { return current; }
    void recall (int scene);
    void copy (int from, int to);

    /** Insert knobs that change with the scenes (every scene starts at the knob's value). */
    bool isSceneControlled (int slot, const juce::String& paramId) const;
    bool setSceneControlled (int slot, const juce::String& paramId, bool controlled);
    /** A removed insert's knobs leave the scenes. */
    void insertRemoved (int slot);

    /** True for a setting or host parameter that scenes hold. */
    static bool isSceneValue (const juce::String& paramId);

    juce::var toVar() const;
    void fromVar (const juce::var&);

    /** Bumped when the current scene or the scene-controlled knobs change. */
    int getVersion() const noexcept { return version.load(); }

private:
    struct Entry
    {
        juce::RangedAudioParameter* param = nullptr;
        int channel = -1, bus = -1;            // the strip it belongs to (recall safe)
        bool glides = true;                    // continuous: glides; a switch or choice: at once
    };

    struct Knob
    {
        int slot = 0;
        juce::String paramId;
    };

    struct Scene
    {
        bool stored = false;
        std::vector<float> values;             // normalised, by entry
        std::array<bool, InsertRack::kTotalSlots> insertOn {};
        std::vector<float> knobs;              // normalised, by knob
    };

    struct Glide
    {
        juce::RangedAudioParameter* param = nullptr;
        float from = 0.0f, to = 0.0f;
    };

    MixSettings& settings;
    InsertRack& rack;
    std::vector<Entry> entries;
    std::vector<Knob> knobs;
    std::array<Scene, kNumScenes> scenes;
    int current = 0;

    std::vector<Glide> glides;
    double glideStart = 0.0, glideMs = 0.0;
    std::atomic<int> version { 0 };

    juce::RangedAudioParameter* knobParam (const Knob&) const;
    bool isRecallSafe (const Entry&) const;
    Scene capture() const;
    void apply (const Scene&);
    void finishGlides();
    void timerCallback() override;
};
