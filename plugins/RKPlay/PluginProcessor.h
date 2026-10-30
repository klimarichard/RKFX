#pragma once

#include "plugin/ProcessorBase.h"

#include "PlayAnalysis.h"
#include "PlayEngine.h"
#include "PlayShow.h"
#include "PlayStream.h"

#include <array>

/** RKPlay: a music player for intermissions and receptions (see docs/design/RKPlay.md).

    Phase 1: one playlist of files, streamed from disk with the next song
    always preloaded, transport with fades, crossfades, safety checks.

    The message thread owns the show and decides what plays: it opens songs
    into free stream slots, starts and arms them in the engine, and follows
    the engine's events. A 30 Hz timer drives that and also acts on the
    transport host parameters (triggers: a widget or footswitch turns one
    on, RKPlay acts and turns it off again). */
class RKPlayProcessor : public rkfx::ProcessorBase,
                        private juce::Timer
{
public:
    enum class State { stopped, playing, paused };

    RKPlayProcessor();
    ~RKPlayProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    //==============================================================================
    // The show (message thread)
    const PlayShow& getShow() const noexcept { return show; }
    juce::AudioFormatManager& getFormats() noexcept { return formats; }

    /** Adds files (folders: their audio files, sorted by name) at @p index (-1: the end). */
    void addFiles (const juce::Array<juce::File>&, int index = -1);
    void removeSong (int index);
    void moveSong (int from, int to);
    void clearSongs();
    void setCrossfade (double seconds);
    void setFadeOut (double seconds);
    void setSmartCrossfade (bool);
    void setSongGain (int index, double db);

    // Loudness
    void setLevelMatch (bool);
    void setTarget (double lufs);
    void setLift (double db);              // 0 (off), 3 or 6
    void setSafetyLimiter (bool);
    PlayLevel levelOf (const PlaySong&) const;
    int analysisPending() const { return analyser != nullptr ? analyser->pending() : 0; }
    float takeLimiterReductionDb() { return engine.takeLimiterReductionDb(); }

    /** Tests: measure every song now (on this thread), and keep the cache in @p file. */
    void analyseNow();
    void useAnalysisCache (const juce::File& file) { cache = std::make_unique<PlayAnalysisCache> (file); }

    /** Tests: streams are filled by update() on the calling thread instead of
        the disk thread, so audio can run faster than real time. */
    void setSynchronousDiskForTesting (bool on) { synchronousDisk = on; }

    /** Bumped when the show changes (the editor polls it). */
    int getShowVersion() const noexcept { return showVersion; }

    //==============================================================================
    // Transport (message thread)
    void play();
    void pause();
    void togglePlay() { getState() == State::playing ? pause() : play(); }
    void next();
    void previous();
    void fadeOut();
    void stop();
    void panic();
    void playSong (int index);           // crossfade into it now (a double-click)
    void seek (double seconds);          // within the current song

    State getState() const noexcept { return state; }
    int getCurrentIndex() const noexcept { return currentIndex; }
    int getNextIndex() const noexcept { return armedIndex >= 0 ? armedIndex : nextPlayable (currentIndex); }
    double getPositionSeconds() const;   // in the current song
    double getLengthSeconds() const;

    /** A short message for the window (a skipped file, a slow disk); bumped version. */
    juce::String getNote() const { return note; }
    int getNoteVersion() const noexcept { return noteVersion; }

    //==============================================================================
    /** Runs the message-thread work once (the timer's job; tests call it directly). */
    void update();

    PlayEngine& getEngine() noexcept { return engine; }   // tests

protected:
    void prepareDsp (double sampleRate, int maxBlockSize, int numChannels) override;
    void resetDsp() override {}
    void processDsp (juce::AudioBuffer<float>&) override;

    juce::var getExtraState() const override;
    void setExtraState (const juce::var&) override;

private:
    juce::AudioFormatManager formats;
    juce::TimeSliceThread diskThread { "RKPlay disk" };
    std::array<PlayStream, PlayEngine::kSlots> streams;
    PlayEngine engine;
    double sampleRate = 0.0;
    juce::AudioBuffer<float> monoScratch;

    // Slots: which song a stream holds, and the last command that named it.
    struct Slot
    {
        bool loaded = false;         // holds a song the processor still means to use
        int song = -1;
        juce::uint32 lastCommand = 0;
        PlayStream::Options options; // how it was opened (re-opened when the analysis or settings change it)
    };
    std::array<Slot, PlayEngine::kSlots> slots;

    PlayShow show;
    std::unique_ptr<PlayAnalysisCache> cache;
    std::unique_ptr<PlayAnalyser> analyser;
    int showVersion = 0;
    State state = State::stopped;
    int currentIndex = 0, currentSlot = -1;
    int armedIndex = -1, armedSlot = -1;
    double armedCrossfade = -1.0, armedLead = 0.0;
    bool underran = false;
    bool synchronousDisk = false;

    juce::String note;
    int noteVersion = 0;

    std::array<juce::RangedAudioParameter*, 6> triggers {};   // play, pause, next, prev, fade, stop
    juce::RangedAudioParameter* levelParam = nullptr;

    void timerCallback() override { update(); }

    /** Opens @p song into a free stream, from @p startSeconds (< 0: where its music starts). */
    int load (int song, double startSeconds);
    PlayStream::Options optionsFor (int song, double startSeconds) const;
    double leadOf (int slot) const;
    void attachAnalyses();
    void takeAnalyses();
    void drop (int& slot);
    void markSent (int slot) { if (slot >= 0) slots[(size_t) slot].lastCommand = engine.commandsSent(); }
    void disarm();
    void keepArmed();
    void handleEvents();
    void pollTriggers();
    void startSong (int index, double crossfade);
    double crossfadeBetween (int a, int b) const;
    int nextPlayable (int from) const;
    int previousPlayable (int from) const;
    void setNote (const juce::String&);
    void showChanged();
};
