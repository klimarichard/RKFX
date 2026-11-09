#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MixInserts.h"
#include "MixSettings.h"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

/** RKMix's 32 scenes, each with a name. A scene holds the mix's values: the faders and
    mutes, every strip and bus setting that is a value (trims, EQ, dynamics,
    pans, sends, assigns, section switches), each insert's on/off, and the
    insert knobs made scene controlled. It never holds the structure (links,
    bus types and formats, tap points, outputs, keys, which plugin is where,
    solo, talkback and oscillator setup, names, colours).

    Recalling a scene stores the current one first (so edits made while a
    scene is active go into it), then glides the continuous values over the
    glide time and switches the rest at once. A recall-safe channel or bus
    keeps its values. A scene never recalled holds what the mix had when it
    was first left.

    Each scene has a recall scope: which kinds of values recalling it
    changes (faders and mutes, pans, the channels' and the buses'
    processing, inserts, group and main assigns) and which buses' sends
    (send levels and send pans to bus n, from channels and buses). A scene
    always stores everything; the scope only filters what a recall applies.
    Message thread. */
class MixScenes : private juce::Timer
{
public:
    static constexpr int kNumScenes = 32;
    static constexpr int kMaxKnobs = 64;

    /** What recalling a scene changes. Everything by default. */
    struct Scope
    {
        enum Category { faders = 0, pans, channelProcessing, busProcessing, inserts, routing, numCategories };
        static constexpr std::uint32_t kAllCategories = (1u << numCategories) - 1;
        static constexpr std::uint32_t kAllSends = (1u << mix::kBuses) - 1;

        std::uint32_t categories = kAllCategories;
        std::uint32_t sendBuses = kAllSends;   // bit n: the sends to bus n

        bool has (Category c) const noexcept     { return (categories >> c) & 1u; }
        bool sendsTo (int bus) const noexcept    { return (sendBuses >> bus) & 1u; }
        void set (Category c, bool on) noexcept  { categories = on ? categories | (1u << c) : categories & ~(1u << c); }
        void setSendsTo (int bus, bool on) noexcept { sendBuses = on ? sendBuses | (1u << bus) : sendBuses & ~(1u << bus); }
        bool isEverything() const noexcept       { return categories == kAllCategories && sendBuses == kAllSends; }
        bool isNothing() const noexcept          { return categories == 0 && sendBuses == 0; }
        bool operator== (const Scope& o) const noexcept { return categories == o.categories && sendBuses == o.sendBuses; }
        bool operator!= (const Scope& o) const noexcept { return ! operator== (o); }

        static juce::String name (Category);
        static juce::String key (Category);
        /** "Everything", "Nothing", or what it recalls ("Sends to Bus 1, Pans"). */
        juce::String describe (const std::function<juce::String (int bus)>& busName) const;
    };

    MixScenes (MixSettings&, juce::AudioProcessor& host, InsertRack&);
    ~MixScenes() override;

    int getCurrent() const noexcept { return current; }
    void recall (int scene);

    /** Undo: makes @p scene the current one again, storing and applying
        nothing (the values come back by themselves), and stops any glide. */
    void restoreCurrent (int scene);
    void cancelGlides();
    void copy (int from, int to);

    /** Insert knobs that change with the scenes (every scene starts at the knob's value). */
    bool isSceneControlled (int slot, const juce::String& paramId) const;
    bool setSceneControlled (int slot, const juce::String& paramId, bool controlled);
    /** A removed insert's knobs leave the scenes. */
    void insertRemoved (int slot);

    /** True for a setting or host parameter that scenes hold. */
    static bool isSceneValue (const juce::String& paramId);

    /** True when a recall with @p scope changes this scene value. */
    static bool isInScope (const Scope&, const juce::String& paramId);

    Scope getScope (int scene) const;
    void setScope (int scene, Scope);

    juce::var toVar() const;
    void fromVar (const juce::var&);

    /** Bumped when the current scene, a name or the scene-controlled knobs change. */
    int getVersion() const noexcept { return version.load(); }

    /** A scene's own name ("" for none), saved with the mix. */
    juce::String getName (int scene) const;
    void setName (int scene, const juce::String&);
    /** "Scene 3", or "Scene 3 Chorus" with its name: the recorder's marker for a recall. */
    juce::String markerLabel (int scene) const;
    /** The scene a recorder marker stands for: a label "Scene N" or "Scene N <anything>"
        (N 1..32) gives N - 1; anything else -1. */
    static int sceneOfMarker (const juce::String& label);
    /** A scene marker's name after "Scene N" ("Scene 3: Hallelujah" gives "Hallelujah"); empty for none. */
    static juce::String nameOfSceneMarker (const juce::String& label);

    //==========================================================================
    /** The scene values, the inserts' on/off and the scene-controlled knobs, as numbers. */
    struct State
    {
        std::vector<float> values;             // normalised, by entry
        std::array<bool, InsertRack::kTotalSlots> insertOn {};
        std::vector<float> knobs;              // normalised, by knob
    };

    /** Recalls replayed without touching the console (export): starts from the
        mix as it is now, then replay() does what recall() would. */
    struct Replay
    {
        State state;
        int current = 0;
        std::array<State, kNumScenes> scenes;
        std::array<bool, kNumScenes> stored {};
    };
    Replay startReplay() const;
    void replay (Replay&, int scene) const;

    int getNumEntries() const noexcept { return (int) entries.size(); }
    juce::RangedAudioParameter* getEntryParam (int index) const noexcept { return entries[(size_t) index].param; }
    bool entryGlides (int index) const noexcept { return entries[(size_t) index].glides; }
    struct KnobRef { int slot = 0; juce::String paramId; bool glides = true; };
    /** The scene-controlled knobs, in State::knobs order. */
    std::vector<KnobRef> getKnobs() const;

private:
    struct Entry
    {
        juce::RangedAudioParameter* param = nullptr;
        int channel = -1, bus = -1;            // the strip it belongs to (recall safe)
        Scope::Category category = Scope::faders;
        int sendBus = -1;                      // a send (level or pan) to this bus: in scope by bus
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
    std::array<Scope, kNumScenes> scopes;      // apart from the scenes: storing a scene keeps its scope
    std::array<juce::String, kNumScenes> names;
    int current = 0;

    std::vector<Glide> glides;
    double glideStart = 0.0, glideMs = 0.0;
    std::atomic<int> version { 0 };

    juce::RangedAudioParameter* knobParam (const Knob&) const;
    bool isRecallSafe (const Entry&) const;
    bool isSlotRecallSafe (int slot) const;
    /** @p state as recalling @p scene (its values) with @p scope leaves it. */
    void applyTo (State& state, const State& scene, const Scope& scope) const;
    Scene capture() const;
    void apply (const Scene&, const Scope&);
    void finishGlides();
    void timerCallback() override;
};
