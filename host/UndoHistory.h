#pragma once

#include <juce_events/juce_events.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rkfx
{

/** The undo history RKMix and RKBoard share: what changed, grouped into
    steps, and how to go back and forth. What is tracked, and how a change is
    made, is the plugin's (a Client).

    Polled ten times a second on the message thread, so a change from
    anywhere (the window, a host widget, a scene recall, an Assign slot, the
    remote) is recorded and no audio-thread listener is needed. Changes that
    follow each other with less than half a second between them are one
    step: a knob drag, a scene recall with its glide.

    A step holds:
    - value changes: a tracked control's value (normalised) before and after;
    - a scene change (undoing a recall makes the scene before it current
      again, without storing anything into either scene);
    - a change of the client's "extra" settings (those that aren't values,
      e.g. RKBoard's assignments), as text, before and after;
    - or a structural change the client recorded itself (pedals added,
      removed or moved), with its own data and description.

    The set of tracked controls may change (pedals come and go): a control
    that appears or disappears is no change. */
class UndoHistory : private juce::Timer
{
public:
    using Key = juce::int64;

    /** What a plugin tracks, and how a step is applied. */
    struct Client
    {
        virtual ~Client() = default;

        /** Every tracked control's value now (normalised), sorted by key. */
        virtual void read (std::vector<std::pair<Key, float>>& values) = 0;
        virtual void write (Key, float value) = 0;
        /** "Delay: Time", "Ch 3 Fader". */
        virtual juce::String nameOf (Key) const = 0;

        /** The current scene (-1: the client has none). */
        virtual int getScene() const { return -1; }
        /** Makes @p scene current without recalling it (nothing stored, nothing moves). */
        virtual void restoreScene (int) {}
        /** "Scene 2", "Scene 2 Verse". */
        virtual juce::String sceneName (int scene) const { return "Scene " + juce::String (scene + 1); }

        /** The extra settings: their version is bumped when they change (then
            they are compared and recorded), their text is read and written. */
        virtual int getExtraVersion() const { return 0; }
        virtual juce::String readExtra() { return {}; }
        virtual void writeExtra (const juce::String&) {}
        virtual juce::String describeExtra (const juce::String& /*before*/, const juce::String& /*after*/) const { return "Settings"; }

        /** A structural step: back to how it was before (@p backwards), or redone. */
        virtual void applyStructure (const juce::var& /*data*/, bool /*backwards*/) {}

        /** Around applying any step (cancel glides, end a morph...). */
        virtual void beginApply() {}
        virtual void endApply() {}
    };

    explicit UndoHistory (Client&);
    ~UndoHistory() override;

    bool canUndo() const;
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void undo();
    void redo();

    /** "Delay: Time", "Scene 2 recall", "12 changes" (empty: nothing). */
    juce::String undoDescription() const;
    juce::String redoDescription() const;

    /** Forgets every step and takes things as they are now (after a load). */
    void clear();

    /** Records what changed since the last poll now (tests; the timer does it otherwise). */
    void poll();
    /** Closes the step being recorded (tests: no waiting for the half second). */
    void closeStep();

    /** Before a structural change: what changed up to now is a step of its own. */
    void beginStructure();
    /** After it: one step, applied by the client's applyStructure(). */
    void endStructure (const juce::var& data, const juce::String& description);

    /** Bumped when the history changes (the window polls it). */
    int getVersion() const noexcept { return version; }

    /** True while a step is being applied. */
    bool isApplying() const noexcept { return applying; }

    static constexpr int kMaxSteps = 200;
    static constexpr double kStepGapMs = 500.0;

private:
    struct Change
    {
        Key key;
        float before, after;
    };
    struct Step
    {
        std::vector<Change> changes;
        int sceneBefore = -1, sceneAfter = -1;
        bool hasExtra = false;
        juce::String extraBefore, extraAfter;
        juce::var structure;
        juce::String label;     // a structural step's description
        bool isStructure() const noexcept { return ! structure.isVoid(); }
        bool empty() const noexcept
        {
            return changes.empty() && sceneBefore == sceneAfter && ! (hasExtra && extraBefore != extraAfter) && ! isStructure();
        }
    };

    Client& client;
    std::vector<std::pair<Key, float>> last, current;
    std::unordered_map<Key, size_t> openSlot;    // a control's place in open.changes
    int lastScene = -1;
    int lastExtraVersion = 0;
    juce::String lastExtra;

    std::deque<Step> undoStack, redoStack;
    Step open;
    bool isOpen = false;
    bool applying = false;
    double lastChangeMs = 0.0;
    int version = 0;

    void timerCallback() override { poll(); }
    void openStep();
    void push (Step&&);
    void resync();
    void apply (const Step&, bool backwards);
    juce::String describe (const Step&) const;
};

} // namespace rkfx
