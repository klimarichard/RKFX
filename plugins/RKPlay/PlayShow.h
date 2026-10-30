#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "PlayAnalysis.h"

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
    Status status = Status::missing;
    double gainDb = 0.0;          // the song's own gain, on top of loudness matching (±12 dB)
    PlayAnalysis analysis;        // valid once measured (or found in the cache)

    bool isPlayable() const noexcept { return status == Status::ok; }
    juce::String displayName() const { return artist.isNotEmpty() ? artist + " - " + title : title; }
};

/** What RKPlay holds (one playlist for now, the transport's times, the
    loudness settings). Message thread only. */
struct PlayShow
{
    static constexpr double kMaxCrossfade = 15.0;

    std::vector<PlaySong> songs;
    double crossfadeSeconds = 6.0;   // 0 = back to back, gapless
    double fadeOutSeconds = 5.0;     // the Fade out button
    double pauseFadeSeconds = 1.0;
    bool smartCrossfade = true;      // songs end at their fade-out point, music start/end skip silence
    PlayLevel::Settings level;       // loudness matching
    bool safetyLimiter = true;

    juce::var toVar() const;
    void fromVar (const juce::var&, juce::AudioFormatManager&);

    /** Checks @p file and reads its length and names (tags, else "Artist - Title" from the file name). */
    static PlaySong makeSong (const juce::File& file, juce::AudioFormatManager&);

    /** Where a song plays from and to (seconds), and when the next one should
        be heard alone (its end point: the fade-out point with Smart crossfade). */
    struct Span { double start = 0.0, end = 0.0, endPoint = 0.0; };
    Span spanOf (const PlaySong&) const;

    /** Checks every song again (a show just loaded, files that may have moved). */
    void checkAll (juce::AudioFormatManager&);

    int numProblems() const;
};
