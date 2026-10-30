#pragma once

#include "plugin/ProcessorBase.h"

#include "PlayAnalysis.h"
#include "PlayEngine.h"
#include "PlayShow.h"
#include "PlayStream.h"

#include <array>
#include <deque>
#include <functional>
#include <random>

/** RKPlay: a music player for intermissions and receptions (see docs/design/RKPlay.md).

    The message thread owns the show and decides what plays: it opens songs
    into free stream slots, starts and arms them in the engine, and follows
    the engine's events. A 30 Hz timer drives that and also acts on the
    transport host parameters (triggers: a widget or footswitch turns one
    on, RKPlay acts and turns it off again).

    What plays is an Item: a song (a copy) and where it came from, a playlist
    or the queue. After the current item comes the queue's first, else the
    playing playlist's next song by its order and At the end setting; each
    playlist remembers where it is while another plays. */
class RKPlayProcessor : public rkfx::ProcessorBase,
                        private juce::Timer
{
public:
    enum class State { stopped, playing, paused };

    /** A song to play, and where it came from. */
    struct Item
    {
        PlaySong song;
        int playlist = -1, index = -1;   // where it is in its playlist (or came from, for a queued one)
        bool queued = false;             // from the queue, or Play now: the playlist's place doesn't move
        int queueId = 0;

        bool isValid() const noexcept { return song.file != juce::File(); }
        bool sameAs (const Item& o) const noexcept
        {
            return queued == o.queued && song.file == o.song.file
                && (queued ? queueId == o.queueId : playlist == o.playlist && index == o.index);
        }
    };

    RKPlayProcessor();
    ~RKPlayProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    //==============================================================================
    // The show (message thread)
    const PlayShow& getShow() const noexcept { return show; }
    juce::AudioFormatManager& getFormats() noexcept { return formats; }

    // Playlists
    int addPlaylist (const juce::String& name = {});
    void removePlaylist (int);
    void renamePlaylist (int, const juce::String&);
    void setOrder (int, PlayPlaylist::Order);
    void setAtEnd (int, PlayPlaylist::AtEnd, int goTo = 0);
    void setStopAfterEach (int, bool);
    void setPlaylistCrossfade (int, double seconds);
    void setPlaylistGain (int, double db);
    bool linkPlaylist (int, const juce::File& folderOrM3u);   // the list becomes the folder's or file's songs
    void unlinkPlaylist (int);                                // keeps the songs as its own
    void rescan (int);
    bool exportPlaylist (int playlist, const juce::File& m3u) const { return show.exportM3u (playlist, m3u); }

    // Songs (a linked playlist's songs come from its folder or file only)
    /** Adds files (folders: their audio files; .m3u8: its songs) to @p playlist at @p index (-1: the end). */
    void addFiles (int playlist, const juce::Array<juce::File>&, int index = -1);
    void addFiles (const juce::Array<juce::File>& files, int index = -1) { addFiles (show.active, files, index); }
    void removeSong (int playlist, int index);
    void moveSong (int playlist, int from, int to);
    void clearSongs (int playlist);
    void setSongGain (int playlist, int index, double db);

    // Show-wide settings
    void setCrossfade (double seconds) { setPlaylistCrossfade (show.active, seconds); }
    void setFadeOut (double seconds);
    void setSmartCrossfade (bool);
    void setLevelMatch (bool);
    void setTarget (double lufs);
    void setLift (double db);              // 0 (off), 3 or 6
    void setSafetyLimiter (bool);
    void setMusicFolder (const juce::File&);
    int relink (const juce::File& folder);
    void setEndAt (bool on, int minutes, bool atSongEnd);

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
    void jumpTo (int playlist, int index);           // a double-click: the playlist goes on from there
    void playNow (int playlist, int index);          // a one-off (a request): the playlist then goes on where it was
    void seek (double seconds);                      // within the current song

    /** Changes the playing playlist: crossfade into its next song now, or after the current song. */
    void switchPlaylist (int playlist, bool now);
    int getPendingSwitch() const noexcept { return pendingSwitch; }

    // The queue
    void queueNext (int playlist, int index);
    void removeQueued (int position);
    void moveQueued (int from, int to);
    void clearQueue();
    const std::deque<Item>& getQueue() const noexcept { return queue; }

    State getState() const noexcept { return state; }
    const Item& getCurrent() const noexcept { return current; }
    Item peekNext();                                 // what plays after the current song (or on Play)
    int getCurrentIndex() const noexcept { return current.isValid() && ! current.queued ? current.index : show.current().cursor; }
    double getPositionSeconds() const;               // in the current song
    double getLengthSeconds() const;

    /** End at: seconds until the music ends (< 0: not set). */
    double getEndAtRemaining() const;

    /** A short message for the window (a skipped file, a slow disk); bumped version. */
    juce::String getNote() const { return note; }
    int getNoteVersion() const noexcept { return noteVersion; }

    // Loudness
    PlayLevel levelOf (const PlaySong&, int playlist) const;
    int analysisPending() const { return analyser != nullptr ? analyser->pending() : 0; }
    float takeLimiterReductionDb() { return engine.takeLimiterReductionDb(); }

    //==============================================================================
    /** Runs the message-thread work once (the timer's job; tests call it directly). */
    void update();

    PlayEngine& getEngine() noexcept { return engine; }   // tests

    /** Tests: measure every song now (on this thread), and keep the cache in @p file. */
    void analyseNow();
    void useAnalysisCache (const juce::File& file) { cache = std::make_unique<PlayAnalysisCache> (file); }

    /** Tests: streams are filled by update() on the calling thread instead of
        the disk thread, so audio can run faster than real time. */
    void setSynchronousDiskForTesting (bool on) { synchronousDisk = on; }

    /** Tests: the clock End at follows. */
    void setClockForTesting (std::function<juce::Time()> c) { clock = std::move (c); }

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

    // Slots: whether a stream holds a song the processor means to use, and the last command that named it.
    struct Slot
    {
        bool loaded = false;
        juce::uint32 lastCommand = 0;
        PlayStream::Options options;     // how it was opened (re-opened when the analysis or settings change it)
        PlaySong song;                   // what it holds (for the smart crossfade's lead)
    };
    std::array<Slot, PlayEngine::kSlots> slots;

    PlayShow show;
    std::unique_ptr<PlayAnalysisCache> cache;
    std::unique_ptr<PlayAnalyser> analyser;
    int showVersion = 0;

    State state = State::stopped;
    Item current, armed;
    int currentSlot = -1, armedSlot = -1;
    double armedCrossfade = -1.0, armedLead = 0.0;
    std::deque<Item> queue;
    int nextQueueId = 1;
    int pendingSwitch = -1;
    bool underran = false;
    bool synchronousDisk = false;
    std::mt19937 random { std::random_device{}() };
    std::vector<bool> cursorPlayed;    // per playlist: its cursor song has been played (Play goes on after it)

    // End at
    std::function<juce::Time()> clock = [] { return juce::Time::getCurrentTime(); };
    juce::Time endAtDeadline;
    bool endingAtSongEnd = false;

    juce::String note;
    int noteVersion = 0;

    std::array<juce::RangedAudioParameter*, 6> triggers {};   // play, pause, next, prev, fade, stop
    juce::RangedAudioParameter *levelParam = nullptr, *playlistParam = nullptr;
    int lastPlaylistParam = 1;

    void timerCallback() override { update(); }

    // Slots
    int load (const Item&, double startSeconds);    // < 0: where its music starts
    PlayStream::Options optionsFor (const Item&, double startSeconds) const;
    double leadOf (int slot) const;
    void drop (int& slot);
    void markSent (int slot) { if (slot >= 0) slots[(size_t) slot].lastCommand = engine.commandsSent(); }

    // What plays next
    Item itemAt (int playlist, int index) const;
    Item following (int playlist, int depth = 0);  // the playlist's next song, by its order and At the end
    Item stepFrom (int playlist, int index, int depth);
    Item previousOf (const Item&);
    void ensureBag (int playlist);
    void commit (const Item&);                     // the item started: playlists and the queue move on
    void startItem (const Item&, double crossfade);
    double crossfadeInto (const Item&) const;
    bool isPlayed (int playlist) const { return playlist >= 0 && playlist < (int) cursorPlayed.size() && cursorPlayed[(size_t) playlist]; }
    void setPlayed (int playlist, bool played);

    void disarm();
    void keepArmed();
    void handleEvents();
    void pollTriggers();
    void checkEndAt();
    void findEndAtDeadline();
    void attachAnalyses();
    void takeAnalyses();
    void playlistEdited (int playlist, const std::function<int (int)>& moveIndex);
    void setNote (const juce::String&);
    void showChanged() { ++showVersion; }
    void edited() { markPresetModified(); showChanged(); }
    void syncPlaylistParam();
};
