#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "midi/MidiSong.h"

#include <array>
#include <atomic>
#include <memory>

/** The RKCue MIDI processing: a sampler whose samples are MIDI files.

    16 slots each hold a MIDI file and a zone of keys (Low to High, with a
    Root). A note-on in a zone starts a copy (a voice) of the slot's file,
    transposed by key - Root when Track Pitch is on. A copy plays on its own
    at the file's tempo map, the host's tempo or a fixed one; the key only
    starts it (One-shot), holds it (Gate) or starts and stops it (Toggle).

    Zone keys are taken in; every other key and all other MIDI pass through
    (Thru). 32 copies in all, up to 8 per slot with Poly (otherwise a new
    trigger restarts the slot); the oldest is stolen. Every note-on sent
    gets its note-off: each copy remembers where its notes went, and a note
    two copies sound together is only ended when the last one ends it.

    GUI-free; songs are built on the message thread and swapped in, and
    processing never allocates. */
class CueEngine
{
public:
    static constexpr int kSlots = 16;
    static constexpr int kVoices = 32;
    static constexpr int kPolyPerSlot = 8;
    static constexpr int kMaxTracks = 16;

    using Song = rkfx::MidiSong;

    enum class Mode { oneShot, gate, toggle };
    enum class TempoSource { file, host, fixed };

    struct Slot
    {
        int low = 36, high = 36, root = 36;
        bool trackPitch = false;
        int transpose = 0;                      // semitones
        bool keepDrums = true;                  // channel 10 is never transposed
        Mode mode = Mode::oneShot;
        bool loop = false;
        bool poly = false;
        float velocity = 1.0f;                  // 0..1: how much the trigger's velocity scales the file's
        int channel = 0;                        // 0 = as recorded, else 1..16
    };

    struct Settings
    {
        std::array<Slot, kSlots> slots;
        TempoSource tempoSource = TempoSource::file;
        double fixedBpm = 120.0;
        double hostBpm = 120.0;
        bool hostHasBpm = false;                // no host tempo: Host plays at the file's
        bool thru = true;
        int triggerChannel = 0;                 // 0 = any, else 1..16
    };

    /** Slot n on the single key C1 + n (C3 = 60), One-shot. */
    static Settings defaultSettings();

    CueEngine();

    void prepare (double sampleRate);
    void setSettings (const Settings& s) noexcept { settings = s; }

    /** Message thread: the file a slot plays (stops the slot). Returns the old one, to delete there. */
    std::unique_ptr<Song> setSong (int slot, std::unique_ptr<Song>);

    // Any thread (host buttons); they act at the start of the next block.
    void play (int slot) noexcept       { playRequests |= bit (slot); }
    void release (int slot) noexcept    { releaseRequests |= bit (slot); }
    void stop (int slot) noexcept       { stopRequests |= bit (slot); }
    void stopAll() noexcept             { stopRequests |= 0xffffu; }

    void process (juce::MidiBuffer&, int numSamples) noexcept;
    void releaseAll (juce::MidiBuffer&) noexcept;

    // For the display and tests (any thread).
    int getCopies (int slot) const noexcept       { return copies[(size_t) slot].load(); }
    float getProgress (int slot) const noexcept   { return progress[(size_t) slot].load(); }   // the newest copy's, 0..1
    bool isKeyDown (int note) const noexcept      { return ((note < 64 ? keysLow : keysHigh).load() >> (note & 63)) & 1u; }
    int getNoteCount() const noexcept             { return noteCount.load(); }                   // note-ons received
    int getLastNote() const noexcept              { return lastNote.load(); }

private:
    static constexpr int kButton = 128;         // a copy started by the slot's Play button

    struct Voice
    {
        bool active = false;
        int slot = 0, key = 0, transpose = 0;
        float velocityScale = 1.0f;
        double position = 0.0;                  // beats
        size_t cursor = 0, tempoIndex = 0;
        long long order = 0;
        int sounding = 0;                       // notes this copy sounds
        std::array<uint16_t, 16 * 128> out {};  // per file (channel, note): (out channel << 8 | out note), 0 = none
    };

    static uint32_t bit (int slot) noexcept { return 1u << (uint32_t) juce::jlimit (0, kSlots - 1, slot); }

    double sampleRate = 48000.0;
    Settings settings;
    juce::MidiBuffer buffer;

    juce::SpinLock songLock;
    std::array<std::unique_ptr<Song>, kSlots> songs;

    std::array<Voice, kVoices> voices;
    std::array<uint8_t, 16 * 128> outCount {};      // copies sounding each (out channel, note)
    std::array<bool, 16 * 128> takenIn {}, passed {};  // incoming notes: triggers / passed through
    long long nextOrder = 0;

    std::atomic<uint32_t> playRequests { 0 }, releaseRequests { 0 }, stopRequests { 0 };
    std::array<std::atomic<int>, kSlots> copies {};
    std::array<std::atomic<float>, kSlots> progress {};
    std::atomic<uint64_t> keysLow { 0 }, keysHigh { 0 };
    std::atomic<int> noteCount { 0 }, lastNote { -1 };

    void handleIncoming (const juce::MidiMessage&, int sample) noexcept;
    void trigger (int slot, int key, int velocity, int sample) noexcept;
    void releaseKey (int slot, int key, int sample) noexcept;
    void stopSlot (int slot, int sample) noexcept;
    void endVoice (Voice&, int sample) noexcept;
    void endNotes (Voice&, int sample) noexcept;
    void advance (Voice&, double toBeat, int sample) noexcept;
    void step (Voice&, int sample) noexcept;
    double tempoOf (Voice&, const Song&) noexcept;
    void noteOn (Voice&, int inChannel, int inNote, int outChannel, int outNote, int velocity, int sample) noexcept;
    void noteOff (uint16_t packed, int sample) noexcept;
    bool inZone (int slot, int key) const noexcept;
    void setKey (int note, bool down) noexcept;
};
