#include "PlayShow.h"

namespace
{
    juce::String metadata (const juce::StringPairArray& values, std::initializer_list<const char*> keys)
    {
        for (const auto* key : keys)
            for (const auto& k : values.getAllKeys())
                if (k.equalsIgnoreCase (key) && values[k].trim().isNotEmpty())
                    return values[k].trim();
        return {};
    }
}

PlaySong PlayShow::makeSong (const juce::File& file, juce::AudioFormatManager& formats)
{
    PlaySong song;
    song.file = file;

    // "Artist - Title" (the archive's naming), or the whole name as the title.
    const auto name = file.getFileNameWithoutExtension();
    const auto dash = name.indexOf (" - ");
    song.title = dash > 0 ? name.substring (dash + 3).trim() : name;
    song.artist = dash > 0 ? name.substring (0, dash).trim() : juce::String();

    if (! file.existsAsFile())
    {
        song.status = PlaySong::Status::missing;
        return song;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        song.status = PlaySong::Status::unreadable;
        return song;
    }

    song.status = PlaySong::Status::ok;
    song.sampleRate = reader->sampleRate;
    song.lengthSeconds = (double) reader->lengthInSamples / reader->sampleRate;

    // Tags where the reader exposes them.
    if (const auto title = metadata (reader->metadataValues, { "title", "TITLE", "INAM" }); title.isNotEmpty())
        song.title = title;
    if (const auto artist = metadata (reader->metadataValues, { "artist", "ARTIST", "IART" }); artist.isNotEmpty())
        song.artist = artist;
    return song;
}

void PlayShow::checkAll (juce::AudioFormatManager& formats)
{
    for (auto& song : songs)
    {
        const auto checked = makeSong (song.file, formats);
        song.status = checked.status;
        song.lengthSeconds = checked.lengthSeconds;
        song.sampleRate = checked.sampleRate;
    }
}

int PlayShow::numProblems() const
{
    int n = 0;
    for (const auto& song : songs)
        n += song.isPlayable() ? 0 : 1;
    return n;
}

juce::var PlayShow::toVar() const
{
    juce::Array<juce::var> list;
    for (const auto& song : songs)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("file", song.file.getFullPathName());
        item->setProperty ("title", song.title);
        if (song.artist.isNotEmpty())
            item->setProperty ("artist", song.artist);
        list.add (juce::var (item));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("songs", list);
    root->setProperty ("crossfade", crossfadeSeconds);
    root->setProperty ("fadeOut", fadeOutSeconds);
    root->setProperty ("pauseFade", pauseFadeSeconds);
    return juce::var (root);
}

void PlayShow::fromVar (const juce::var& v, juce::AudioFormatManager& formats)
{
    songs.clear();
    crossfadeSeconds = juce::jlimit (0.0, kMaxCrossfade, (double) v.getProperty ("crossfade", 6.0));
    fadeOutSeconds = juce::jlimit (0.5, 30.0, (double) v.getProperty ("fadeOut", 5.0));
    pauseFadeSeconds = juce::jlimit (0.05, 5.0, (double) v.getProperty ("pauseFade", 1.0));

    if (const auto* list = v["songs"].getArray())
        for (const auto& item : *list)
        {
            const auto path = item["file"].toString();
            if (! juce::File::isAbsolutePath (path))
                continue;
            auto song = makeSong (juce::File (path), formats);
            // The names stored with the show win (a song renamed in the show).
            if (item.hasProperty ("title"))
                song.title = item["title"].toString();
            if (item.hasProperty ("artist"))
                song.artist = item["artist"].toString();
            songs.push_back (std::move (song));
        }
}
