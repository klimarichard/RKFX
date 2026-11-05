#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MixInserts.h"
#include "MixMonitorEq.h"

#include <optional>

class RKMixProcessor;

/** Monitor EQ's flow on the message thread (docs/design/RKMix Monitor EQ.md),
    shared by the console panel and the remote: the options, the steps of a
    measurement, the response and the suggestion, Apply and Undo.

        idle -> floor (3 s, silent) -> settling (0.5 s) -> measuring (5 s)
             -> waiting (move the mic; Next) -> settling ... -> done

    The last position finishes by itself; Finish takes what is measured so
    far. Stop ends a run at once; so do a scene recall, the bus stopping
    being an Aux bus, and the audio restarting. A loaded measurement file
    (In-ear) goes straight to done.

    Apply writes the suggestion into the bus's first RKGraphEQ (switched to
    31 bands and on), inserting one in the first free slot when there is
    none. Undo puts back what Apply changed (one step).

    Listeners hear every change (a step, the live bands, the options). */
class MixMonitorEqSession : public juce::ChangeBroadcaster,
                            private juce::Timer
{
public:
    using Bands = monitoreq::Bands;
    using Flags = monitoreq::Flags;

    enum class Step { idle, floor, settling, measuring, waiting, done };
    /** In-ear's sides: Each ear alternates left and right by position. */
    enum class Sides { both, left, right, eachEar };

    static constexpr double kFloorSeconds = 3.0, kSettleSeconds = 0.5, kPositionSeconds = 5.0;
    static constexpr float kClipLevel = 0.98f;
    static constexpr int kMaxPositions = 8;
    static constexpr float kEarDifferenceWarningDb = 3.0f;

    struct Options
    {
        monitoreq::Type type = monitoreq::Type::speaker;
        int input = 0;                                   // 0-based
        monitoreq::Mic mic = monitoreq::Mic::flat;
        bool useCalibration = false;                     // the loaded calibration file instead of mic
        juce::String calibrationName;
        Bands calibration {};
        monitoreq::Target speakerTarget = monitoreq::Target::gentleTilt;
        monitoreq::Target inEarTarget = monitoreq::Target::speakersInRoom;
        float bassDb = 4.0f;
        float speakerLevelDb = -20.0f, inEarLevelDb = -40.0f;
        Sides sides = Sides::both;                       // In-ear
        int speakerPositions = 4, inEarPositions = 2;
        float strength = 0.75f;
        bool useFile = false;                            // In-ear: a loaded measurement instead of measuring
        juce::String fileName;
        Bands file {};
        bool hasFile = false, hasCalibration = false;
    };

    static constexpr float kMaxInEarLevelDb = -20.0f;

    explicit MixMonitorEqSession (RKMixProcessor&);
    ~MixMonitorEqSession() override;

    //==============================================================================
    const Options& getOptions() const noexcept { return options; }
    /** New options: clamped; the suggestion follows. Not while a run measures (type, input, sides wait). */
    void setOptions (const Options&);

    monitoreq::Target currentTarget() const noexcept;
    float currentLevelDb() const noexcept;
    int currentPositions() const noexcept;
    Sides currentSides() const noexcept { return options.type == monitoreq::Type::inEar ? options.sides : Sides::both; }
    const Bands& currentMicCurve() const noexcept;

    /** Reads a calibration or measurement file; false (and a message) when it isn't one. */
    bool loadCalibration (const juce::File&);
    bool loadMeasurementFile (const juce::File&);

    //==============================================================================
    /** Starts on Aux bus @p bus (or, In-ear with Use file, takes the file). False with a message when it can't. */
    bool start (int bus);
    /** Waiting: measures the next position. */
    void next();
    /** Waiting: done with the positions measured so far (at least one). */
    void finish();
    /** Ends a run (the bus handed back), with @p why as the message; a finished result stays. */
    void stop (const juce::String& why = {});

    /** Who started the run from a remote (a console's token; empty: the window). Cleared by start(). */
    void setOwner (const juce::String& token) { owner = token; }
    const juce::String& getOwner() const noexcept { return owner; }

    /** Bumped on every change (the remote polls it). */
    int getVersion() const noexcept { return version; }
    /** Back to idle, forgetting the result. */
    void clear();

    Step getStep() const noexcept { return step; }
    bool isRunning() const noexcept { return step != Step::idle && step != Step::done; }
    int getBus() const noexcept { return bus; }
    int positionsMeasured() const noexcept { return (int) positions.size(); }
    /** 0..1 through the current timed step (floor, settling, measuring). */
    float getProgress() const noexcept { return progress; }
    /** What the user should know now (the step's instruction, a warning); empty for none. */
    juce::String getMessage() const { return message; }
    /** Instructions for the current step ("Position 2 of 4: ..."). */
    juce::String describeStep() const;
    /** The side the current or next position measures (Each ear). */
    MixMeasure::Sides positionSides (int position) const noexcept;

    /** The response while measuring (the window so far) or at the end, normalised; and what has data. */
    const monitoreq::Suggestion& getSuggestion() const noexcept { return suggestion; }
    bool hasResult() const noexcept { return step == Step::done && hasResponse; }
    /** Each ear: the largest left-right difference (dB), or 0. */
    float getEarDifferenceDb() const noexcept { return earDifference; }
    /** The bus's GraphEQ gains as measured through (31 bands; zeros without one). */
    const Bands& getMeasuredGains() const noexcept { return measuredGains; }

    //==============================================================================
    /** Apply is possible: a result, and a GraphEQ on the bus or a free slot. Else why not. */
    bool canApply() const;
    juce::String applyProblem() const;
    void apply();
    bool canUndo() const noexcept { return undoState.has_value(); }
    void undo();

    /** The bus's first RKGraphEQ slot, or -1; the first free slot, or -1. */
    int findGraphEq (int busIndex) const;
    int findFreeSlot (int busIndex) const;

    /** The 31-band gains a GraphEQ in @p slot of @p busIndex applies now (its 10 or 15 bands
        read at the 31 centres; zeros when it's off). */
    Bands graphEqGains (int busIndex, int slot) const;

    //==============================================================================
    juce::var toVar() const;
    void fromVar (const juce::var&);

    /** Tests: one timer tick now. */
    void tick() { timerCallback(); }

private:
    RKMixProcessor& processor;
    Options options;

    Step step = Step::idle;
    int bus = -1, scene = -1;
    bool pendingStart = false;             // the last run is still handing the bus back
    MixMeasure::Sums mark;                 // the start of the current timed step
    bool marked = false;
    float progress = 0.0f;
    juce::String message;
    monitoreq::Window floor;
    std::vector<monitoreq::Window> positions;
    std::vector<int> positionSide;         // the MixMeasure::Sides of each position
    bool hasResponse = false;
    monitoreq::Response response;
    monitoreq::Suggestion suggestion;
    Bands measuredGains {};
    float earDifference = 0.0f;

    struct UndoState
    {
        int bus = -1, slot = -1;
        bool inserted = false, wasOn = true;
        float mode = 0.0f;
        Bands gains {};
    };
    std::optional<UndoState> undoState;

    MixMeasure& measure() const;
    void timerCallback() override;
    void beginPosition();
    void endRun (const juce::String& why);
    void computeResult();
    void updateSuggestion();
    void live (const monitoreq::Window&);
    bool readFile (const juce::File&, Bands&, juce::String& name);
    juce::String owner;
    int version = 0;
    void changed() { ++version; sendChangeMessage(); }
};
