#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

/** The RKConduct MIDI processing: a MIDI file played by tapping its beats.

    Each tap says "we are at the next tap point now" (every Tap Every note
    value, e.g. every quarter note). Between taps the file plays at the
    tempo measured from the taps, so finer notes (a 1/16 pattern) keep their
    spacing; the player never runs past the next tap point:

    - a late tap: the player waits at the tap point (sounding notes keep
      sounding) and goes on when the tap comes;
    - an early tap: it jumps to the tap point: the notes in between are
      skipped (notes already sounding end, other events still go out) and
      the faster tempo is taken from then on.

    The first tap starts the file (after Count-in taps, if set) at the Tempo
    setting, or the tempo measured from the count-in; from the second tap on
    the tempo is the taps'. A second tap sooner than a third of a beat is
    ignored (a double trigger). At the end the file stops, or loops.

    Taps come from two keys (either one; they are taken in, the rest of the
    keyboard passes through), a MIDI controller, or tap() (a host
    parameter). Every track has its own on/off and output channel; a
    sounding note's note-off always goes where its note-on went. GUI-free;
    the song is built on the message thread and swapped in, and processing
    never allocates. */
class ConductEngine
{
public:
    static constexpr int kMaxTracks = 16;

    struct TrackInfo
    {
        juce::String name;
        int notes = 0;
    };

    /** A MIDI file, ready to play: every track's events in beats. */
    struct Song
    {
        struct Event { double beat; int track; juce::MidiMessage message; };
        std::vector<Event> events;              // by beat
        std::vector<TrackInfo> tracks;          // up to kMaxTracks
        double lengthBeats = 0.0;
        double tempoBpm = 120.0;                // the file's first tempo
    };

    /** Reads a Standard MIDI File (nullptr when it isn't one). Tracks
        beyond kMaxTracks are left out. */
    static std::unique_ptr<Song> makeSong (const juce::MidiFile&);
    static std::unique_ptr<Song> makeSong (const juce::MemoryBlock& fileData);

    struct Settings
    {
        double tapBeats = 1.0;                  // quarter notes per tap
        double tempoBpm = 120.0;                // before the taps give one
        int countIn = 0;                        // taps before the start
        bool loop = false;
        int key1 = 36, key2 = 38;               // tap keys, -1 = off
        int tapController = -1;                 // tap CC, -1 = off
        std::array<bool, kMaxTracks> trackOn {};
        std::array<int, kMaxTracks> trackChannel {};   // 0 = as recorded, else 1..16
    };

    ConductEngine();

    void prepare (double sampleRate);
    void setSettings (const Settings& s) noexcept { settings = s; }

    /** Message thread: the song to play (stops the current one). Returns the old one, to delete there. */
    std::unique_ptr<Song> setSong (std::unique_ptr<Song>);

    // Audio thread, before process().
    void tap() noexcept       { pendingTaps++; }
    void stop() noexcept      { stopRequested = true; }

    void process (juce::MidiBuffer&, int numSamples) noexcept;
    void releaseAll (juce::MidiBuffer&) noexcept;

    // For the display and tests (any thread).
    bool isPlaying() const noexcept          { return playing.load(); }
    double getPosition() const noexcept      { return shownPosition.load(); }
    double getTempo() const noexcept         { return shownTempo.load(); }
    int getTapsToStart() const noexcept      { return tapsToStart.load(); }
    bool isWaiting() const noexcept          { return waiting.load(); }

private:
    double sampleRate = 48000.0;
    Settings settings;
    juce::MidiBuffer out;

    juce::SpinLock songLock;
    std::unique_ptr<Song> song;

    std::atomic<bool> playing { false }, waiting { false };
    std::atomic<double> shownPosition { 0.0 }, shownTempo { 120.0 };
    std::atomic<int> tapsToStart { 0 }, pendingTaps { 0 };
    std::atomic<bool> stopRequested { false };

    double position = 0.0, nextTapPoint = 1.0, tempo = 120.0;
    size_t cursor = 0;
    long long now = 0, lastTap = -1;
    double lastInterval = 0.0;
    int countedTaps = 0;
    bool ccDown = false;

    std::array<std::array<uint8_t, 128>, kMaxTracks> soundingChannel {};   // 0 = not sounding, else 1..16

    void onTap (int position) noexcept;
    void advanceTo (double beat, int sample, bool skipping) noexcept;
    void endAll (int sample) noexcept;
    void stopNow (int sample) noexcept;
    bool isTapKey (int note) const noexcept { return note >= 0 && (note == settings.key1 || note == settings.key2); }
};
