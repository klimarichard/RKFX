#include "PlayShow.h"

#include <algorithm>
#include <map>

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

    const char* orderKey (PlayPlaylist::Order o) { return o == PlayPlaylist::Order::shuffle ? "shuffle" : "order"; }
    const char* atEndKey (PlayPlaylist::AtEnd a)
    {
        return a == PlayPlaylist::AtEnd::stop ? "stop" : a == PlayPlaylist::AtEnd::goTo ? "goto" : "repeat";
    }
    const char* linkKey (PlayPlaylist::Link l) { return l == PlayPlaylist::Link::folder ? "folder" : l == PlayPlaylist::Link::m3u ? "m3u" : "none"; }
}

PlayShow::PlayShow()
{
    playlists.push_back ({});
    playlists.back().name = "Playlist 1";
}

//==============================================================================
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
    song.fileSize = file.getSize();

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

bool PlayShow::isPlaylistFile (const juce::File& f)
{
    return f.hasFileExtension ("m3u;m3u8");
}

juce::Array<juce::File> PlayShow::filesIn (const juce::File& source, bool subfolders, juce::AudioFormatManager& formats)
{
    juce::Array<juce::File> files;
    if (source.isDirectory())
    {
        files = source.findChildFiles (juce::File::findFiles, subfolders, formats.getWildcardForAllFormats());
        files.sort();
        return files;
    }

    // An .m3u/.m3u8: one path per line, relative to the file's folder; # lines are comments.
    juce::StringArray lines;
    lines.addLines (source.loadFileAsString());
    for (auto line : lines)
    {
        line = line.trim();
        if (line.isEmpty() || line.startsWithChar ('#'))
            continue;
        if (line.startsWithIgnoreCase ("file://"))
            line = juce::URL (line).getLocalFile().getFullPathName();
        const auto f = juce::File::isAbsolutePath (line) ? juce::File (line) : source.getParentDirectory().getChildFile (line);
        files.add (f);   // missing ones too: the safety check marks them
    }
    return files;
}

void PlayShow::rescan (int index, juce::AudioFormatManager& formats)
{
    auto& p = playlists[(size_t) index];
    if (! p.isLinked())
        return;

    std::map<juce::String, double> gains;
    for (const auto& s : p.songs)
        if (std::abs (s.gainDb) > 0.001)
            gains[s.file.getFullPathName()] = s.gainDb;

    p.songs.clear();
    if (p.linkFile.exists())
        for (const auto& f : filesIn (p.linkFile, p.linkSubfolders, formats))
        {
            auto song = makeSong (f, formats);
            if (const auto it = gains.find (f.getFullPathName()); it != gains.end())
                song.gainDb = it->second;
            p.songs.push_back (std::move (song));
        }
    p.cursor = juce::jlimit (0, juce::jmax (0, (int) p.songs.size() - 1), p.cursor);
    p.bag.clear();
    p.bagPos = -1;
}

bool PlayShow::exportM3u (int index, const juce::File& file) const
{
    const auto& p = playlists[(size_t) index];
    juce::String text ("#EXTM3U\n");
    for (const auto& s : p.songs)
    {
        text << "#EXTINF:" << juce::roundToInt (s.lengthSeconds) << "," << s.displayName() << "\n";
        text << (s.file.isAChildOf (file.getParentDirectory()) ? s.file.getRelativePathFrom (file.getParentDirectory())
                                                                : s.file.getFullPathName()) << "\n";
    }
    return file.replaceWithText (text, false, false, "\n");
}

int PlayShow::setMusicFolder (const juce::File& folder, juce::AudioFormatManager& formats)
{
    const auto old = musicFolder;
    musicFolder = folder;
    if (old == juce::File() || folder == juce::File() || old == folder)
        return 0;

    int found = 0;
    for (auto& p : playlists)
        for (auto& s : p.songs)
            if (s.status == PlaySong::Status::missing && s.file.isAChildOf (old))
                if (const auto moved = folder.getChildFile (s.file.getRelativePathFrom (old)); moved.existsAsFile())
                {
                    const auto gain = s.gainDb;
                    const auto title = s.title, artist = s.artist;
                    s = makeSong (moved, formats);
                    s.gainDb = gain;
                    s.title = title;
                    s.artist = artist;
                    ++found;
                }
    return found;
}

int PlayShow::relink (const juce::File& folder, juce::AudioFormatManager& formats)
{
    // Every audio file under the folder, by name.
    std::multimap<juce::String, juce::File> byName;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, formats.getWildcardForAllFormats()))
        byName.insert ({ f.getFileName().toLowerCase(), f });

    int found = 0;
    for (auto& p : playlists)
        for (auto& s : p.songs)
        {
            if (s.status != PlaySong::Status::missing)
                continue;
            const auto [from, to] = byName.equal_range (s.file.getFileName().toLowerCase());
            if (from == to)
                continue;
            // Several with that name: the one of the same size.
            auto pick = from->second;
            for (auto it = from; it != to; ++it)
                if (s.fileSize > 0 && it->second.getSize() == s.fileSize)
                    pick = it->second;
            const auto gain = s.gainDb;
            const auto title = s.title, artist = s.artist;
            s = makeSong (pick, formats);
            s.gainDb = gain;
            s.title = title;
            s.artist = artist;
            ++found;
        }
    return found;
}

//==============================================================================
PlayShow::Span PlayShow::spanOf (const PlaySong& song) const
{
    Span span { 0.0, song.lengthSeconds, song.lengthSeconds };
    if (smartCrossfade && song.analysis.valid)
    {
        span.start = song.analysis.musicStart;
        span.end = juce::jmax (span.start, song.analysis.musicEnd);
        span.endPoint = juce::jlimit (span.start, span.end, song.analysis.fadeOutPoint);
    }
    return span;
}

int PlayShow::numProblems() const
{
    int n = 0;
    for (const auto& p : playlists)
        for (const auto& song : p.songs)
            n += song.isPlayable() ? 0 : 1;
    return n;
}

void PlayShow::reshuffle (PlayPlaylist& p, int avoid, std::mt19937& random)
{
    p.bag.clear();
    for (int i = 0; i < (int) p.songs.size(); ++i)
        p.bag.push_back (i);
    std::shuffle (p.bag.begin(), p.bag.end(), random);
    // The last song of one round never starts the next.
    if (p.bag.size() > 1 && p.bag.front() == avoid)
        std::swap (p.bag.front(), p.bag.back());
    p.bagPos = -1;
}

//==============================================================================
juce::var PlayShow::toVar() const
{
    juce::Array<juce::var> lists;
    for (const auto& p : playlists)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", p.name);
        o->setProperty ("order", orderKey (p.order));
        o->setProperty ("atEnd", atEndKey (p.atEnd));
        o->setProperty ("goTo", p.goTo + 1);
        o->setProperty ("stopAfterEach", p.stopAfterEach);
        o->setProperty ("crossfade", p.crossfadeSeconds);
        o->setProperty ("gain", p.gainDb);
        o->setProperty ("link", linkKey (p.link));
        if (p.isLinked())
        {
            o->setProperty ("linkFile", p.linkFile.getFullPathName());
            o->setProperty ("subfolders", p.linkSubfolders);
        }

        juce::Array<juce::var> songs;
        for (const auto& song : p.songs)
        {
            auto* item = new juce::DynamicObject();
            item->setProperty ("file", song.file.getFullPathName());
            // Inside the Music folder: also relative to it (the show moves with the folder).
            if (musicFolder.isDirectory() && song.file.isAChildOf (musicFolder))
                item->setProperty ("rel", song.file.getRelativePathFrom (musicFolder));
            item->setProperty ("title", song.title);
            if (song.artist.isNotEmpty())
                item->setProperty ("artist", song.artist);
            if (std::abs (song.gainDb) > 0.001)
                item->setProperty ("gain", song.gainDb);
            if (song.fileSize > 0)
                item->setProperty ("size", song.fileSize);
            songs.add (juce::var (item));
        }
        o->setProperty ("songs", songs);
        lists.add (juce::var (o));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("playlists", lists);
    root->setProperty ("active", active + 1);
    root->setProperty ("fadeOut", fadeOutSeconds);
    root->setProperty ("pauseFade", pauseFadeSeconds);
    root->setProperty ("smartCrossfade", smartCrossfade);
    root->setProperty ("levelMatch", level.match);
    root->setProperty ("target", level.targetLufs);
    root->setProperty ("peakCeiling", level.peakCeilingDb);
    root->setProperty ("lift", level.liftDb);
    root->setProperty ("safetyLimiter", safetyLimiter);
    if (musicFolder != juce::File())
        root->setProperty ("musicFolder", musicFolder.getFullPathName());
    root->setProperty ("endAtOn", endAt.on);
    root->setProperty ("endAt", juce::String (endAt.minutes / 60).paddedLeft ('0', 2) + ":" + juce::String (endAt.minutes % 60).paddedLeft ('0', 2));
    root->setProperty ("endAtSongEnd", endAt.atSongEnd);

    auto* d = new juce::DynamicObject();
    d->setProperty ("auto", duck.automatic);
    d->setProperty ("depth", duck.depthDb);
    d->setProperty ("threshold", duck.thresholdDb);
    d->setProperty ("attack", duck.attackSeconds);
    d->setProperty ("hold", duck.holdSeconds);
    d->setProperty ("release", duck.releaseSeconds);
    root->setProperty ("duck", juce::var (d));
    root->setProperty ("cueLevel", cueLevelDb);

    juce::Array<juce::var> padList;
    for (const auto& pad : pads)
    {
        auto* o = new juce::DynamicObject();
        if (! pad.isEmpty())
        {
            o->setProperty ("file", pad.file.getFullPathName());
            if (musicFolder.isDirectory() && pad.file.isAChildOf (musicFolder))
                o->setProperty ("rel", pad.file.getRelativePathFrom (musicFolder));
            o->setProperty ("name", pad.name);
            o->setProperty ("level", pad.levelDb);
            o->setProperty ("loop", pad.loop);
            o->setProperty ("ducks", pad.ducksMusic);
            o->setProperty ("stops", pad.stopsMusic);
            o->setProperty ("colour", pad.colour);
        }
        padList.add (juce::var (o));
    }
    root->setProperty ("pads", padList);
    return juce::var (root);
}

void PlayShow::fromVar (const juce::var& v, juce::AudioFormatManager& formats)
{
    *this = {};
    fadeOutSeconds = juce::jlimit (0.5, 30.0, (double) v.getProperty ("fadeOut", 5.0));
    pauseFadeSeconds = juce::jlimit (0.05, 5.0, (double) v.getProperty ("pauseFade", 1.0));
    smartCrossfade = v.getProperty ("smartCrossfade", true);
    level.match = v.getProperty ("levelMatch", true);
    level.targetLufs = juce::jlimit (-23.0, -14.0, (double) v.getProperty ("target", -18.0));
    level.peakCeilingDb = juce::jlimit (-3.0, 0.0, (double) v.getProperty ("peakCeiling", -0.5));
    level.liftDb = juce::jlimit (0.0, 6.0, (double) v.getProperty ("lift", 3.0));
    safetyLimiter = v.getProperty ("safetyLimiter", true);
    if (const auto folder = v["musicFolder"].toString(); juce::File::isAbsolutePath (folder))
        musicFolder = juce::File (folder);
    endAt.on = v.getProperty ("endAtOn", false);
    endAt.atSongEnd = v.getProperty ("endAtSongEnd", false);
    if (const auto t = juce::StringArray::fromTokens (v["endAt"].toString(), ":", {}); t.size() == 2)
        endAt.minutes = juce::jlimit (0, 24 * 60 - 1, t[0].getIntValue() * 60 + t[1].getIntValue());

    const auto& d = v["duck"];
    duck.automatic = d.getProperty ("auto", false);
    duck.depthDb = juce::jlimit (-30.0, 0.0, (double) d.getProperty ("depth", -12.0));
    duck.thresholdDb = juce::jlimit (-60.0, 0.0, (double) d.getProperty ("threshold", -35.0));
    duck.attackSeconds = juce::jlimit (0.05, 1.0, (double) d.getProperty ("attack", 0.2));
    duck.holdSeconds = juce::jlimit (0.0, 5.0, (double) d.getProperty ("hold", 1.0));
    duck.releaseSeconds = juce::jlimit (0.2, 5.0, (double) d.getProperty ("release", 1.5));
    cueLevelDb = juce::jlimit (-24.0, 6.0, (double) v.getProperty ("cueLevel", 0.0));

    if (const auto* padList = v["pads"].getArray())
        for (int i = 0; i < juce::jmin (kPads, padList->size()); ++i)
        {
            const auto& item = padList->getReference (i);
            auto& pad = pads[(size_t) i];
            auto file = juce::File (item["file"].toString());
            if (const auto rel = item["rel"].toString(); rel.isNotEmpty() && musicFolder != juce::File())
                if (const auto inFolder = musicFolder.getChildFile (rel); inFolder.existsAsFile() || ! file.existsAsFile())
                    file = inFolder;
            if (! juce::File::isAbsolutePath (item["file"].toString()))
                continue;
            pad.file = file;
            pad.name = item["name"].toString();
            pad.levelDb = juce::jlimit (-24.0, 12.0, (double) item.getProperty ("level", 0.0));
            pad.loop = item.getProperty ("loop", false);
            pad.ducksMusic = item.getProperty ("ducks", true);
            pad.stopsMusic = item.getProperty ("stops", false);
            pad.colour = juce::jlimit (0, 7, (int) item.getProperty ("colour", 0));
        }

    const auto readSongs = [&] (PlayPlaylist& p, const juce::var& list)
    {
        if (const auto* items = list.getArray())
            for (const auto& item : *items)
            {
                // Relative to the Music folder when it has the song; else the absolute path.
                auto file = juce::File (item["file"].toString());
                if (const auto rel = item["rel"].toString(); rel.isNotEmpty() && musicFolder != juce::File())
                    if (const auto inFolder = musicFolder.getChildFile (rel); inFolder.existsAsFile() || ! file.existsAsFile())
                        file = inFolder;
                if (! juce::File::isAbsolutePath (file.getFullPathName()))
                    continue;
                auto song = makeSong (file, formats);
                // The names stored with the show win (a song renamed in the show).
                if (item.hasProperty ("title"))
                    song.title = item["title"].toString();
                if (item.hasProperty ("artist"))
                    song.artist = item["artist"].toString();
                song.gainDb = juce::jlimit (-12.0, 12.0, (double) item.getProperty ("gain", 0.0));
                if (song.fileSize == 0)
                    song.fileSize = (juce::int64) item.getProperty ("size", 0);
                p.songs.push_back (std::move (song));
            }
    };

    playlists.clear();
    if (const auto* lists = v["playlists"].getArray())
    {
        for (const auto& item : *lists)
        {
            if ((int) playlists.size() >= kMaxPlaylists)
                break;
            PlayPlaylist p;
            p.name = item["name"].toString();
            p.order = item["order"].toString() == "shuffle" ? PlayPlaylist::Order::shuffle : PlayPlaylist::Order::inOrder;
            const auto atEnd = item["atEnd"].toString();
            p.atEnd = atEnd == "stop" ? PlayPlaylist::AtEnd::stop : atEnd == "goto" ? PlayPlaylist::AtEnd::goTo : PlayPlaylist::AtEnd::repeat;
            p.goTo = juce::jlimit (0, kMaxPlaylists - 1, (int) item.getProperty ("goTo", 1) - 1);
            p.stopAfterEach = item.getProperty ("stopAfterEach", false);
            p.crossfadeSeconds = juce::jlimit (0.0, kMaxCrossfade, (double) item.getProperty ("crossfade", 6.0));
            p.gainDb = juce::jlimit (-12.0, 12.0, (double) item.getProperty ("gain", 0.0));
            const auto link = item["link"].toString();
            p.link = link == "folder" ? PlayPlaylist::Link::folder : link == "m3u" ? PlayPlaylist::Link::m3u : PlayPlaylist::Link::none;
            p.linkSubfolders = item.getProperty ("subfolders", true);
            if (const auto path = item["linkFile"].toString(); juce::File::isAbsolutePath (path))
                p.linkFile = juce::File (path);
            else
                p.link = PlayPlaylist::Link::none;
            readSongs (p, item["songs"]);
            if (p.name.isEmpty())
                p.name = "Playlist " + juce::String ((int) playlists.size() + 1);
            playlists.push_back (std::move (p));
            // A linked list is read again from its folder or file.
            if (playlists.back().isLinked())
                rescan ((int) playlists.size() - 1, formats);
        }
    }
    else if (v.hasProperty ("songs"))
    {
        // A show from before playlists (one list at the top level).
        PlayPlaylist p;
        p.name = "Playlist 1";
        p.crossfadeSeconds = juce::jlimit (0.0, kMaxCrossfade, (double) v.getProperty ("crossfade", 6.0));
        readSongs (p, v["songs"]);
        p.cursor = (int) v.getProperty ("current", 0);
        playlists.push_back (std::move (p));
    }

    if (playlists.empty())
    {
        playlists.push_back ({});
        playlists.back().name = "Playlist 1";
    }
    active = juce::jlimit (0, (int) playlists.size() - 1, (int) v.getProperty ("active", 1) - 1);
    for (auto& p : playlists)
    {
        p.goTo = juce::jlimit (0, (int) playlists.size() - 1, p.goTo);
        p.cursor = juce::jlimit (0, juce::jmax (0, (int) p.songs.size() - 1), p.cursor);
    }
}
