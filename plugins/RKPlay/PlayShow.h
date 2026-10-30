#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "PlayAnalysis.h"

#include <random>
#include <vector>

/** One song in a playlist: its file, the names shown, and what the safety
    check found. */
struct PlaySong
{
    enum class Status { ok, missing, unreadable };

    juce::File file;
    juce::String title, artist;
    double lengthSeconds = 0.0;
    double sampleRate = 0.0;
    juce::int64 fileSize = 0;     // for relinking: a moved file is found by name, checked by size
    Status status = Status::missing;
    double gainDb = 0.0;          // the song's own gain, on top of loudness matching (±12 dB)
    PlayAnalysis analysis;        // valid once measured (or found in the cache)

    bool isPlayable() const noexcept { return status == Status::ok; }
    juce::String displayName() const { return artist.isNotEmpty() ? artist + " - " + title : title; }
};

/** A playlist: its songs (its own, or a linked folder's or .m3u8 file's),
    how it plays, and where it is (kept while another playlist plays). */
struct PlayPlaylist
{
    enum class Order { inOrder, shuffle };
    enum class AtEnd { stop, repeat, goTo };
    enum class Link { none, folder, m3u };

    juce::String name;
    std::vector<PlaySong> songs;
    Order order = Order::inOrder;
    AtEnd atEnd = AtEnd::repeat;
    int goTo = 0;                    // the playlist after this one (AtEnd::goTo)
    bool stopAfterEach = false;      // wait for Play after every song
    double crossfadeSeconds = 6.0;   // 0 = back to back, gapless
    double gainDb = 0.0;             // the whole list, on top of loudness matching (a quieter Dinner list)
    Link link = Link::none;
    juce::File linkFile;             // the folder or the .m3u8
    bool linkSubfolders = true;

    // Where it is (not saved): the song now playing from it, or the one Play starts.
    int cursor = 0;
    std::vector<int> bag;            // shuffle: this round's order
    int bagPos = -1;                 // the cursor's place in the bag
    std::vector<int> nextBag;        // the next round's order, once the end of this one is near

    bool isLinked() const noexcept { return link != Link::none; }
};

/** What RKPlay holds: up to 8 playlists, the transport's times, the loudness
    settings, the Music folder and End at. Message thread only. */
struct PlayShow
{
    static constexpr int kMaxPlaylists = 8;
    static constexpr double kMaxCrossfade = 15.0;

    std::vector<PlayPlaylist> playlists;
    int active = 0;                  // the playlist Play and Next work on (the one playing)
    double fadeOutSeconds = 5.0;     // the Fade out button
    double pauseFadeSeconds = 1.0;
    bool smartCrossfade = true;      // songs end at their fade-out point, music start/end skip silence
    PlayLevel::Settings level;       // loudness matching
    bool safetyLimiter = true;

    /** Songs inside it are stored relative to it, so a show works wherever the folder is. */
    juce::File musicFolder;

    struct EndAt
    {
        bool on = false;
        int minutes = 22 * 60;       // the clock time, minutes after midnight
        bool atSongEnd = false;      // else exactly at the time (a fade out)
    } endAt;

    PlayShow();

    PlayPlaylist& current() { return playlists[(size_t) juce::jlimit (0, (int) playlists.size() - 1, active)]; }
    const PlayPlaylist& current() const { return playlists[(size_t) juce::jlimit (0, (int) playlists.size() - 1, active)]; }

    juce::var toVar() const;
    void fromVar (const juce::var&, juce::AudioFormatManager&);

    /** Checks @p file and reads its length and names (tags, else "Artist - Title" from the file name). */
    static PlaySong makeSong (const juce::File& file, juce::AudioFormatManager&);

    /** The audio files of a folder (sorted by path), or of an .m3u/.m3u8 file (in its order). */
    static juce::Array<juce::File> filesIn (const juce::File& folderOrPlaylist, bool subfolders, juce::AudioFormatManager&);
    static bool isPlaylistFile (const juce::File&);

    /** Reads a linked playlist's folder or file again, keeping each song's gain. */
    void rescan (int playlist, juce::AudioFormatManager&);

    /** Writes @p playlist as an .m3u8 with paths relative to @p file. */
    bool exportM3u (int playlist, const juce::File& file) const;

    /** Sets the Music folder. If it moved (another Mac, another user name),
        missing songs that were inside the old one are found again by their
        path inside the new one; returns how many. */
    int setMusicFolder (const juce::File& folder, juce::AudioFormatManager&);

    /** Finds missing songs under @p folder by file name (and size); returns how many were found. */
    int relink (const juce::File& folder, juce::AudioFormatManager&);

    /** Where a song plays from and to (seconds), and when the next one should
        be heard alone (its end point: the fade-out point with Smart crossfade). */
    struct Span { double start = 0.0, end = 0.0, endPoint = 0.0; };
    Span spanOf (const PlaySong&) const;

    int numProblems() const;

    /** Shuffle: a new round's order for @p playlist, never starting with @p avoid. */
    static void reshuffle (PlayPlaylist&, int avoid, std::mt19937&);
};
