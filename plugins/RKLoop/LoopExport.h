#pragma once

#include "LoopEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <optional>
#include <vector>

/** RKLoop Export: the loops as WAV files (docs/design/RKLoop Export.md).

    The loop exactly as stored (forward, at normal speed, its seam fades,
    the overdubs as they are now, not the Loop Level), at the session's
    rate and the plugin's channel count, 24-bit or 32-bit float, with a
    smpl chunk looping the whole file. Nothing here runs on the audio
    thread: the copies are taken on the message thread (LoopEngine::copyLoop)
    and the files are written on a background thread. */
namespace LoopExport
{
    enum class Format { int24, float32 };

    /** ~/Music/RKMusic Plugins/RKFX/Loops: where Quick save puts its folders. */
    juce::File loopsFolder();

    /** "2026-11-06 21-14" */
    juce::String stamp (juce::Time);

    /** " 120 BPM", " 92.5 BPM" (rounded to 0.1), or nothing without a tempo. */
    juce::String bpmSuffix (std::optional<double> bpm);

    /** One loop on its own: "RKLoop 2026-11-06 21-14 Loop 3 Layer 120 BPM.wav" (@p loop 0..7). */
    juce::String singleFileName (juce::Time, int loop, bool layer, std::optional<double> bpm);

    /** Export all: the folder "RKLoop 2026-11-06 21-14", and in it "Loop 1 Part.wav"... */
    juce::String folderName (juce::Time);
    juce::String fileInFolderName (int loop, bool layer);

    /** Writes @p audio as a WAV file with a smpl chunk (one forward loop,
        0 to length - 1), replacing @p file. */
    juce::Result writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate, Format);

    /** One loop's copy, ready to be written. */
    struct Take
    {
        int loop = 0;
        juce::File file;
        juce::AudioBuffer<float> audio;
    };

    /** Writes every take (creating their folders); the message for the
        status line: "Saved 4 loops", "Saved Loop 3", or the first error. */
    juce::String writeTakes (std::vector<Take>&, double sampleRate, Format, const juce::StringArray& skipped);
}

/** Runs the exports of one RKLoop (owned by the processor; message thread).

    A request names the loops and their files. Each 100 ms it tries to copy
    the loops still missing; a loop that's busy (recording, overdubbing, an
    Undo swap) is retried for up to 5 s, then left out and named. The copies
    are written by one background job; the processor's destructor waits for
    it (the pool is a member). The Save Loops footswitch is counted from any
    thread and acted on here. */
class LoopExporter : private juce::Timer
{
public:
    struct Context
    {
        std::function<int()> numChannels;                 // 1 or 2
        std::function<double()> sampleRate;
        std::function<std::optional<double>()> bpm;       // the host's, if it gives one
        std::function<bool (int)> isLayer;
    };

    LoopExporter (const LoopEngine&, Context);
    ~LoopExporter() override;

    /** Loops with something in them (busy or not), in order. */
    std::vector<int> loopsWithAudio() const;

    void exportLoop (int loop, const juce::File& file);
    void exportAll (const juce::File& parentFolder);      // into a new "RKLoop <date> <time>" folder in it
    void quickSave();                                     // into the Loops folder, no dialog

    /** The Save Loops footswitch (any thread, also the audio thread): one press. */
    void requestQuickSave() noexcept { ++quickSaveRequests; }
    int getPendingQuickSaves() const noexcept { return quickSaveRequests.load(); }   // tests

    bool isRunning() const noexcept;                      // collecting or writing

    /** The status line ("Saved 4 loops", an error), shown for a few seconds; empty when there is none. */
    juce::String getStatus() const;
    bool statusIsError() const noexcept { return statusError; }

    // Remembered per machine (not in the preset).
    LoopExport::Format getFormat() const noexcept { return format; }
    void setFormat (LoopExport::Format);
    juce::File getLastFolder() const;
    void setLastFolder (const juce::File&);

    /** Tests: the retry time for a busy loop, and no settings file. */
    void setRetrySeconds (double s) noexcept { retrySeconds = s; }
    static void useSettingsFile (bool) noexcept;

    /** Tests: one timer tick at once. */
    void tick() { timerCallback(); }
    void waitForWriter() const;    // until the files being written are finished

private:
    const LoopEngine& engine;
    Context context;
    LoopExport::Format format = LoopExport::Format::int24;
    double retrySeconds = 5.0;

    struct Request
    {
        std::vector<LoopExport::Take> wanted;     // their files; the audio filled in once copied
        std::vector<bool> copied;
        juce::Time started;
        double sampleRate = 0.0;
    };
    std::optional<Request> request;

    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withThreadName ("RKLoop Export").withNumberOfThreads (1) };
    std::atomic<bool> writing { false };
    std::atomic<int> quickSaveRequests { 0 };

    juce::CriticalSection resultLock;
    juce::String writtenResult;                   // from the writer, picked up by the timer
    bool hasWrittenResult = false;

    juce::String status;
    bool statusError = false;
    juce::Time statusTime;

    std::unique_ptr<juce::PropertiesFile> settings;

    void start (std::vector<LoopExport::Take>);
    void setStatus (const juce::String&, bool error);
    void timerCallback() override;
};
