#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MixInserts.h"
#include "MixSettings.h"

#include <array>

/** RKMix's 32 assignables: host parameters "Assign 1" to "Assign 32", each
    driving one control of the console (a mix setting or a fader or mute) or
    of an insert, between a minimum and a maximum, optionally inverted. Two
    ways: moving the control moves its Assign parameter too, so a host widget
    stays in step. A switch is on at 50% and above.

    Assignments change on the message thread; the parameters can move on any
    thread (host automation), so the table is read under a spin lock. */
class MixAssigns : private juce::AudioProcessorParameter::Listener
{
public:
    static constexpr int kNumAssigns = 32;

    struct Target
    {
        enum class Kind { none, mix, insert };   // mix: a setting or a host parameter of RKMix
        Kind kind = Kind::none;
        juce::String paramId;
        int slot = -1;                           // insert: its slot number
        float min = 0.0f, max = 1.0f;            // normalised range of the target
        bool invert = false;
    };

    MixAssigns (MixSettings&, juce::AudioProcessor& host, InsertRack&);
    ~MixAssigns() override;

    Target get (int assign) const;
    /** Assigns @p assign to a control (full range); a control has at most one Assign: assigning it again moves it. */
    void assign (int assign, Target::Kind, const juce::String& paramId, int slot = -1);
    void clear (int assign);
    void setRange (int assign, float min, float max, bool invert);
    /** The Assign driving this control, or -1. */
    int find (Target::Kind, const juce::String& paramId, int slot = -1) const;
    /** An insert removed: its assignments go. */
    void insertRemoved (int slot);

    /** "Vocal: Fader", "Bus 2, insert 1: Mix", or empty. */
    std::function<juce::String (const Target&)> describeTarget;
    juce::String describe (int assign) const;

    juce::var toVar() const;
    void fromVar (const juce::var&);

    int getVersion() const noexcept { return version.load(); }

private:
    MixSettings& settings;
    juce::AudioProcessor& host;
    InsertRack& rack;

    std::array<Target, kNumAssigns> targets;
    std::array<juce::RangedAudioParameter*, kNumAssigns> assignParams {};
    std::array<juce::RangedAudioParameter*, kNumAssigns> targetParams {};   // resolved, under lock
    juce::SpinLock lock;
    std::atomic<int> version { 0 };
    static inline thread_local bool following = false;   // this thread is moving a control or an Assign for the other

    juce::RangedAudioParameter* resolve (const Target&) const;
    void attach (int assign);
    void detach (int assign);

    struct Follower : public juce::AudioProcessorParameter::Listener
    {
        Follower (MixAssigns& o, int a) : owner (o), index (a) {}
        void parameterValueChanged (int, float value) override { owner.targetMoved (index, value); }
        void parameterGestureChanged (int, bool) override {}
        MixAssigns& owner;
        const int index;
    };
    std::array<std::unique_ptr<Follower>, kNumAssigns> followers;

    void targetMoved (int assign, float normalised);
    void parameterValueChanged (int parameterIndex, float newValue) override;   // an Assign parameter moved
    void parameterGestureChanged (int, bool) override {}
};
