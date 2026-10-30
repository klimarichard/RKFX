#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <deque>
#include <map>
#include <vector>

/** What RKPlay measures once per song (on a background thread), and keeps
    in a cache: its loudness and peaks, where the music really starts and
    ends, where a long fade-out has gone quiet, and a waveform overview. */
struct PlayAnalysis
{
    static constexpr float kSilenceDb = -50.0f;      // music starts and ends where it first and last passes this
    static constexpr double kFadeOutBelowLu = 20.0;  // the fade-out point: quieter than the song by this much, for good
    static constexpr int kOverviewPoints = 1000;

    bool valid = false;
    double loudness = -200.0;            // integrated, LUFS
    float truePeakDb = -200.0f, samplePeakDb = -200.0f;
    double lengthSeconds = 0.0;
    double musicStart = 0.0, musicEnd = 0.0;
    double fadeOutPoint = 0.0;           // <= musicEnd
    std::vector<juce::uint8> overview;   // peak per point, 0..255 linear

    /** Measures the whole file (stops early, invalid, if @p cancel turns true). */
    static PlayAnalysis analyse (juce::AudioFormatReader&, const std::atomic<bool>* cancel = nullptr);

    juce::var toVar() const;
    static PlayAnalysis fromVar (const juce::var&);
};

/** How loud a song plays, from its analysis and the show's loudness settings. */
struct PlayLevel
{
    struct Settings
    {
        bool match = true;
        double targetLufs = -18.0;
        double peakCeilingDb = -0.5;     // dBTP: pure gain never lifts a song's true peak above it
        double liftDb = 3.0;             // up to this much more, limiting its peaks (0 = off)
    };

    static constexpr double kOnTargetDb = 0.1;   // a gain smaller than this is none (bit-exact)

    double gainDb = 0.0;                 // applied to the song
    double limitDb = 0.0;                // > 0: the song is lifted past its ceiling and limited by up to this

    static PlayLevel plan (const PlayAnalysis&, const Settings&, double songGainDb);
};

/** The analysis cache: Application Support/RKFX/RKPlay/analysis.json, keyed
    by a file's path, size and modification date, so a show of 500 songs
    opens at once and a changed file is measured again. Message thread. */
class PlayAnalysisCache
{
public:
    explicit PlayAnalysisCache (juce::File file);

    static juce::File defaultFile();

    bool find (const juce::File&, PlayAnalysis&) const;
    void store (const juce::File&, const PlayAnalysis&);
    void save();   // if anything changed

private:
    juce::File file;
    std::map<juce::String, juce::var> entries;
    bool dirty = false;

    static juce::String keyFor (const juce::File&);
};

/** The background analyser: a low-priority thread working through a queue of
    files; results are collected by the message thread with takeResults(). */
class PlayAnalyser : private juce::Thread
{
public:
    PlayAnalyser();
    ~PlayAnalyser() override;

    void add (const juce::File&);
    int pending() const;

    struct Result
    {
        juce::File file;
        PlayAnalysis analysis;
    };
    std::vector<Result> takeResults();

private:
    juce::AudioFormatManager formats;
    mutable juce::CriticalSection lock;
    std::deque<juce::File> queue;
    std::vector<Result> results;
    juce::File working;
    std::atomic<bool> cancelFlag { false };
    juce::WaitableEvent wake;

    void run() override;
};
