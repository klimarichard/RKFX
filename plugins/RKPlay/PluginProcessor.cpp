#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>

namespace
{
    rkfx::PluginConfig makeConfig()
    {
        rkfx::PluginConfig c;
        c.name = "RKPlay";
        c.stateVersion = 1;
        c.hasMix = false;
        c.hasOutput = false;                  // its own Level (a host parameter)
        c.midiInput = true;                   // MIDI learn and program changes (later phases)
        c.extraInPresets = true;              // a preset is the whole show
        c.bypassMutes = true;                 // the input is only the duck key: never passed on
        c.accent = juce::Colour (0xff6fa58a); // Utility family, sage
        c.family = rkfx::Palette::Family::utility;
        return c;
    }

    rkfx::ParamBuilder makeParams()
    {
        rkfx::ParamBuilder p;
        // Transport triggers: a widget or footswitch turns one on, RKPlay acts and turns it off.
        p.addBool ("play", "Play", false);
        p.addBool ("pause", "Pause", false);
        p.addBool ("next", "Next", false);
        p.addBool ("prev", "Previous", false);
        p.addBool ("fade", "Fade Out", false);
        p.addBool ("stop", "Stop", false);
        p.addDecibels ("level", "Level", -60.0f, 6.0f, 0.0f);   // -60 = silence
        return p;
    }

    constexpr double kStopFade = 0.1, kPanicFade = 0.02, kSeekFade = 0.05, kRestartFade = 0.3;
    constexpr double kPrefillSeconds = 1.0;    // read at once when a song is opened
    constexpr double kRestartAfter = 5.0;      // Previous restarts the song after this much of it
}

RKPlayProcessor::RKPlayProcessor()
    : rkfx::ProcessorBase (makeConfig(), makeParams())
{
    formats.registerBasicFormats();
    const char* ids[] = { "play", "pause", "next", "prev", "fade", "stop" };
    for (size_t i = 0; i < triggers.size(); ++i)
        triggers[i] = findParam (ids[i]);
    levelParam = findParam ("level");

    for (int slot = 0; slot < PlayEngine::kSlots; ++slot)
        engine.setStream (slot, &streams[(size_t) slot]);
    diskThread.startThread (juce::Thread::Priority::high);
    cache = std::make_unique<PlayAnalysisCache> (PlayAnalysisCache::defaultFile());
    analyser = std::make_unique<PlayAnalyser>();
    startTimerHz (30);
}

RKPlayProcessor::~RKPlayProcessor()
{
    stopTimer();
    analyser.reset();
    if (cache != nullptr)
        cache->save();
    for (auto& stream : streams)
        diskThread.removeTimeSliceClient (&stream);
    diskThread.stopThread (2000);
}

//==============================================================================
void RKPlayProcessor::prepareDsp (double newRate, int maxBlockSize, int /*numChannels*/)
{
    monoScratch.setSize (2, maxBlockSize);

    // Same rate: keep playing (a host may prepare again for a new block size).
    if (juce::exactlyEqual (newRate, sampleRate))
    {
        engine.prepareBlock (maxBlockSize);
        return;
    }

    // A new rate: the streams resample for it, so everything starts again.
    for (auto& stream : streams)
        diskThread.removeTimeSliceClient (&stream);
    sampleRate = newRate;
    for (auto& stream : streams)
        stream.prepare (sampleRate);
    engine.prepare (sampleRate, maxBlockSize);
    slots = {};
    currentSlot = armedSlot = armedIndex = -1;
    state = State::stopped;
}

void RKPlayProcessor::processDsp (juce::AudioBuffer<float>& buffer)
{
    // The input is only ever the duck key: the output is the music alone.
    buffer.clear();
    if (sampleRate <= 0.0)
        return;
    const auto n = buffer.getNumSamples();
    if (buffer.getNumChannels() >= 2)
    {
        engine.process (buffer.getArrayOfWritePointers(), n);
        return;
    }
    monoScratch.clear (0, n);
    engine.process (monoScratch.getArrayOfWritePointers(), n);
    buffer.copyFrom (0, 0, monoScratch, 0, 0, n);
    buffer.addFrom (0, 0, monoScratch, 1, 0, n);
    buffer.applyGain (0, 0, n, 0.5f);
}

//==============================================================================
// Slots

int RKPlayProcessor::load (int song, double startSeconds)
{
    if (song < 0 || song >= (int) show.songs.size() || sampleRate <= 0.0)
        return -1;

    int slot = -1;
    for (int s = 0; s < PlayEngine::kSlots && slot < 0; ++s)
        if (! slots[(size_t) s].loaded && engine.isSlotIdle (s, slots[(size_t) s].lastCommand))
            slot = s;
    if (slot < 0)
        return -1;   // every stream busy (a burst of skips): the next tick tries again

    auto& entry = show.songs[(size_t) song];
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (entry.file));
    if (reader == nullptr)
    {
        entry.status = entry.file.existsAsFile() ? PlaySong::Status::unreadable : PlaySong::Status::missing;
        setNote ("Skipped " + entry.displayName() + ": the file is " + (entry.file.existsAsFile() ? "unreadable" : "missing"));
        showChanged();
        return -1;
    }

    const auto options = optionsFor (song, startSeconds);
    auto& stream = streams[(size_t) slot];
    diskThread.removeTimeSliceClient (&stream);
    stream.open (std::move (reader), options);
    if (synchronousDisk)
    {
        stream.fillNow();
    }
    else
    {
        stream.prefill (kPrefillSeconds);
        diskThread.addTimeSliceClient (&stream);
    }
    slots[(size_t) slot] = { true, song, engine.commandsSent(), options };
    return slot;
}

PlayLevel RKPlayProcessor::levelOf (const PlaySong& song) const
{
    return PlayLevel::plan (song.analysis, show.level, song.gainDb);
}

PlayStream::Options RKPlayProcessor::optionsFor (int song, double startSeconds) const
{
    const auto& entry = show.songs[(size_t) song];
    const auto span = show.spanOf (entry);
    const auto level = levelOf (entry);
    PlayStream::Options o;
    o.startSeconds = startSeconds < 0.0 ? span.start : startSeconds;
    o.endSeconds = entry.analysis.valid && show.smartCrossfade ? span.end : -1.0;
    o.gainDb = level.gainDb;
    o.limitCeilingDb = level.limitDb > 0.0 ? show.level.peakCeilingDb : 1.0;
    return o;
}

double RKPlayProcessor::leadOf (int slot) const
{
    // Smart crossfade: the song is over at its fade-out point, before the stream's end.
    const auto song = slots[(size_t) slot].song;
    if (! show.smartCrossfade || song < 0 || song >= (int) show.songs.size())
        return 0.0;
    const auto& entry = show.songs[(size_t) song];
    const auto& o = slots[(size_t) slot].options;
    const auto streamEnd = o.endSeconds < 0.0 ? entry.lengthSeconds : o.endSeconds;
    return entry.analysis.valid ? juce::jmax (0.0, streamEnd - entry.analysis.fadeOutPoint) : 0.0;
}

void RKPlayProcessor::drop (int& slot)
{
    if (slot >= 0)
        slots[(size_t) slot].loaded = false;
    slot = -1;
}

void RKPlayProcessor::disarm()
{
    if (armedSlot >= 0)
    {
        engine.disarm();
        markSent (armedSlot);
    }
    drop (armedSlot);
    armedIndex = -1;
    armedCrossfade = -1.0;
    armedLead = 0.0;
}

//==============================================================================
// What plays next

int RKPlayProcessor::nextPlayable (int from) const
{
    const auto n = (int) show.songs.size();
    for (int i = 1; i <= n; ++i)
    {
        const auto index = ((from + i) % n + n) % n;   // the playlist repeats
        if (show.songs[(size_t) index].isPlayable())
            return index;
    }
    return -1;
}

int RKPlayProcessor::previousPlayable (int from) const
{
    const auto n = (int) show.songs.size();
    for (int i = 1; i <= n; ++i)
    {
        const auto index = ((from - i) % n + n) % n;
        if (show.songs[(size_t) index].isPlayable())
            return index;
    }
    return -1;
}

double RKPlayProcessor::crossfadeBetween (int a, int b) const
{
    // Never longer than half of either song.
    auto seconds = show.crossfadeSeconds;
    for (const auto i : { a, b })
        if (i >= 0 && i < (int) show.songs.size())
        {
            const auto span = show.spanOf (show.songs[(size_t) i]);
            seconds = juce::jmin (seconds, (span.endPoint - span.start) * 0.5);
        }
    return juce::jmax (0.0, seconds);
}

void RKPlayProcessor::keepArmed()
{
    if (state == State::stopped || currentSlot < 0)
        return;

    const auto want = nextPlayable (currentIndex);
    if (want < 0)
    {
        disarm();
        return;
    }
    const auto seconds = crossfadeBetween (currentIndex, want);

    const auto lead = leadOf (currentSlot);

    // Already armed with that song, opened as it should be now (the analysis
    // or the loudness settings may have changed how).
    if (armedSlot >= 0 && armedIndex == want && slots[(size_t) armedSlot].song == want)
    {
        const auto& have = slots[(size_t) armedSlot].options;
        const auto wantOptions = optionsFor (want, -1.0);
        const auto same = juce::approximatelyEqual (have.startSeconds, wantOptions.startSeconds)
                       && juce::approximatelyEqual (have.endSeconds, wantOptions.endSeconds)
                       && juce::approximatelyEqual (have.gainDb, wantOptions.gainDb)
                       && juce::approximatelyEqual (have.limitCeilingDb, wantOptions.limitCeilingDb);
        if (same)
        {
            if (! juce::exactlyEqual (seconds, armedCrossfade) || ! juce::exactlyEqual (lead, armedLead))
            {
                engine.arm (armedSlot, seconds, 1.0f, lead);
                markSent (armedSlot);
                armedCrossfade = seconds;
                armedLead = lead;
            }
            return;
        }
    }

    disarm();
    const auto slot = load (want, -1.0);
    if (slot < 0)
        return;
    engine.arm (slot, seconds, 1.0f, lead);
    markSent (slot);
    armedSlot = slot;
    armedIndex = want;
    armedCrossfade = seconds;
    armedLead = lead;
}

void RKPlayProcessor::startSong (int index, double crossfade)
{
    const auto slot = load (index, -1.0);
    if (slot < 0)
    {
        if (show.songs.empty() || index < 0)
            return;
        // Unreadable now (load said why): go on to the one after it.
        const auto after = nextPlayable (index);
        if (after >= 0 && after != index && after != currentIndex)
            startSong (after, crossfade);
        return;
    }

    disarm();
    if (state == State::stopped)
        engine.start (slot, 0.01, 1.0f);
    else
        engine.crossfadeTo (slot, state == State::paused ? kSeekFade : crossfade, 1.0f);
    markSent (slot);
    markSent (currentSlot);
    drop (currentSlot);
    currentSlot = slot;
    currentIndex = index;
    state = State::playing;
    underran = false;
    keepArmed();
    showChanged();
}

//==============================================================================
// Transport

void RKPlayProcessor::play()
{
    if (state == State::paused)
    {
        engine.resume (show.pauseFadeSeconds);
        state = State::playing;
        showChanged();
        return;
    }
    if (state == State::playing || show.songs.empty())
        return;

    currentIndex = juce::jlimit (0, (int) show.songs.size() - 1, currentIndex);
    const auto index = show.songs[(size_t) currentIndex].isPlayable() ? currentIndex : nextPlayable (currentIndex);
    if (index < 0)
    {
        setNote ("Nothing to play: every file is missing or unreadable");
        return;
    }
    startSong (index, 0.0);
}

void RKPlayProcessor::pause()
{
    if (state != State::playing)
        return;
    engine.pause (show.pauseFadeSeconds);
    state = State::paused;
    showChanged();
}

void RKPlayProcessor::next()
{
    const auto index = nextPlayable (currentIndex);
    if (index < 0)
        return;
    if (state == State::stopped)
    {
        currentIndex = index;
        showChanged();
        return;
    }
    startSong (index, crossfadeBetween (currentIndex, index));
}

void RKPlayProcessor::previous()
{
    if (state != State::stopped && getPositionSeconds() > kRestartAfter)
    {
        startSong (currentIndex, kRestartFade);
        return;
    }
    const auto index = previousPlayable (currentIndex);
    if (index < 0)
        return;
    if (state == State::stopped)
    {
        currentIndex = index;
        showChanged();
        return;
    }
    startSong (index, crossfadeBetween (currentIndex, index));
}

void RKPlayProcessor::fadeOut()
{
    if (state == State::stopped)
        return;
    disarm();
    engine.fadeOutAll (show.fadeOutSeconds);
    markSent (currentSlot);
    drop (currentSlot);
    state = State::stopped;
    // Play goes on with the next song.
    if (const auto index = nextPlayable (currentIndex); index >= 0)
        currentIndex = index;
    showChanged();
}

void RKPlayProcessor::stop()
{
    if (state == State::stopped)
        return;
    disarm();
    engine.fadeOutAll (kStopFade);
    markSent (currentSlot);
    drop (currentSlot);
    state = State::stopped;
    showChanged();
}

void RKPlayProcessor::panic()
{
    disarm();
    engine.fadeOutAll (kPanicFade);
    markSent (currentSlot);
    drop (currentSlot);
    state = State::stopped;
    showChanged();
}

void RKPlayProcessor::playSong (int index)
{
    if (index < 0 || index >= (int) show.songs.size() || ! show.songs[(size_t) index].isPlayable())
        return;
    startSong (index, state == State::playing ? crossfadeBetween (currentIndex, index) : 0.0);
}

void RKPlayProcessor::seek (double seconds)
{
    if (state == State::stopped || currentSlot < 0)
        return;
    const auto slot = load (currentIndex, seconds);
    if (slot < 0)
        return;
    disarm();
    engine.crossfadeTo (slot, kSeekFade, 1.0f);
    if (state == State::paused)
        engine.pause (0.0);   // stay paused at the new place
    markSent (slot);
    markSent (currentSlot);
    drop (currentSlot);
    currentSlot = slot;
    keepArmed();
}

double RKPlayProcessor::getPositionSeconds() const
{
    return currentSlot >= 0 ? streams[(size_t) currentSlot].getPositionSeconds() : 0.0;
}

double RKPlayProcessor::getLengthSeconds() const
{
    return currentIndex >= 0 && currentIndex < (int) show.songs.size() ? show.songs[(size_t) currentIndex].lengthSeconds : 0.0;
}

//==============================================================================
// The message thread's regular work

void RKPlayProcessor::update()
{
    if (synchronousDisk)
        for (auto& stream : streams)
            stream.fillNow();

    pollTriggers();
    handleEvents();
    takeAnalyses();
    keepArmed();
    engine.setSafetyLimiter (show.safetyLimiter);

    const auto db = valueOf (levelParam);
    engine.setLevel (db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db));
}

void RKPlayProcessor::pollTriggers()
{
    for (size_t i = 0; i < triggers.size(); ++i)
    {
        auto* p = triggers[i];
        if (p->getValue() < 0.5f)
            continue;
        p->setValueNotifyingHost (0.0f);
        switch (i)
        {
            case 0: play(); break;
            case 1: pause(); break;
            case 2: next(); break;
            case 3: previous(); break;
            case 4: fadeOut(); break;
            case 5: stop(); break;
            default: break;
        }
    }
}

void RKPlayProcessor::handleEvents()
{
    PlayEngine::Event e;
    while (engine.popEvent (e))
    {
        switch (e.type)
        {
            case PlayEngine::Event::Type::advanced:
                if (e.slot == armedSlot)
                {
                    drop (currentSlot);
                    currentSlot = armedSlot;
                    currentIndex = armedIndex;
                    armedSlot = -1;
                    armedIndex = -1;
                    armedCrossfade = -1.0;
                    showChanged();
                }
                break;

            case PlayEngine::Event::Type::underrun:
                if (e.slot == currentSlot)
                {
                    underran = true;
                    setNote ("The disk couldn't keep up with " + show.songs[(size_t) currentIndex].displayName()
                             + ": went on to the next song");
                }
                break;

            case PlayEngine::Event::Type::ended:
                if (e.slot == currentSlot)
                {
                    // The current song ended with nothing armed after it (or ran dry).
                    drop (currentSlot);
                    const auto wasPlaying = state == State::playing;
                    state = State::stopped;
                    const auto after = nextPlayable (currentIndex);
                    if (wasPlaying && underran && after >= 0)
                        startSong (after, 0.0);
                    else if (after >= 0)
                        currentIndex = after;
                    underran = false;
                    showChanged();
                }
                break;

            case PlayEngine::Event::Type::released:
                break;
        }
    }
}

//==============================================================================
// The show

void RKPlayProcessor::addFiles (const juce::Array<juce::File>& files, int index)
{
    const auto wildcard = formats.getWildcardForAllFormats();
    juce::Array<juce::File> found;
    for (const auto& f : files)
    {
        if (f.isDirectory())
        {
            auto inFolder = f.findChildFiles (juce::File::findFiles, true, wildcard);
            inFolder.sort();
            found.addArray (inFolder);
        }
        else if (f.existsAsFile())
        {
            found.add (f);
        }
    }
    if (found.isEmpty())
        return;

    index = index < 0 || index > (int) show.songs.size() ? (int) show.songs.size() : index;
    std::vector<PlaySong> added;
    for (const auto& f : found)
        added.push_back (PlayShow::makeSong (f, formats));
    show.songs.insert (show.songs.begin() + index, added.begin(), added.end());

    // The songs after the insert point moved down (an empty list just starts at its first song).
    const auto wasEmpty = show.songs.size() == added.size();
    const auto shift = [&] (int& i) { if (! wasEmpty && i >= index) i += (int) added.size(); };
    shift (currentIndex);
    shift (armedIndex);
    for (auto& s : slots)
        shift (s.song);
    attachAnalyses();

    if (const auto problems = (int) std::count_if (added.begin(), added.end(), [] (const PlaySong& s) { return ! s.isPlayable(); }))
        setNote (juce::String (problems) + (problems == 1 ? " file" : " files") + " can't be played (marked in red)");
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::removeSong (int index)
{
    if (index < 0 || index >= (int) show.songs.size())
        return;
    if (index == currentIndex && state != State::stopped)
        return;   // the playing song stays (the window doesn't offer it)

    show.songs.erase (show.songs.begin() + index);
    const auto shift = [index] (int& i) { if (i > index) --i; else if (i == index) i = -1; };
    if (currentIndex > index)
        --currentIndex;
    shift (armedIndex);
    for (auto& s : slots)
        shift (s.song);
    currentIndex = juce::jlimit (0, juce::jmax (0, (int) show.songs.size() - 1), currentIndex);
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::moveSong (int from, int to)
{
    const auto n = (int) show.songs.size();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    auto song = std::move (show.songs[(size_t) from]);
    show.songs.erase (show.songs.begin() + from);
    show.songs.insert (show.songs.begin() + to, std::move (song));

    const auto moved = [from, to] (int i)
    {
        if (i == from) return to;
        if (from < to && i > from && i <= to) return i - 1;
        if (from > to && i >= to && i < from) return i + 1;
        return i;
    };
    currentIndex = moved (currentIndex);
    if (armedIndex >= 0)
        armedIndex = moved (armedIndex);
    for (auto& s : slots)
        if (s.song >= 0)
            s.song = moved (s.song);
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::clearSongs()
{
    if (state != State::stopped)
        stop();
    show.songs.clear();
    currentIndex = 0;
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::setCrossfade (double seconds)
{
    show.crossfadeSeconds = juce::jlimit (0.0, PlayShow::kMaxCrossfade, seconds);
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::setFadeOut (double seconds)
{
    show.fadeOutSeconds = juce::jlimit (0.5, 30.0, seconds);
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::setSmartCrossfade (bool on)
{
    show.smartCrossfade = on;
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::setSongGain (int index, double db)
{
    if (index < 0 || index >= (int) show.songs.size())
        return;
    show.songs[(size_t) index].gainDb = juce::jlimit (-12.0, 12.0, db);
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::setLevelMatch (bool on)
{
    show.level.match = on;
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::setTarget (double lufs)
{
    show.level.targetLufs = juce::jlimit (-23.0, -14.0, lufs);
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::setLift (double db)
{
    show.level.liftDb = juce::jlimit (0.0, 6.0, db);
    markPresetModified();
    showChanged();
}

void RKPlayProcessor::setSafetyLimiter (bool on)
{
    show.safetyLimiter = on;
    markPresetModified();
    showChanged();
}

//==============================================================================
// Analysis: from the cache at once, the rest measured in the background

void RKPlayProcessor::attachAnalyses()
{
    for (auto& song : show.songs)
    {
        if (song.analysis.valid || ! song.isPlayable())
            continue;
        if (cache == nullptr || ! cache->find (song.file, song.analysis))
            if (analyser != nullptr)
                analyser->add (song.file);
    }
}

void RKPlayProcessor::takeAnalyses()
{
    if (analyser == nullptr)
        return;
    auto results = analyser->takeResults();
    if (results.empty())
        return;
    for (auto& r : results)
    {
        if (! r.analysis.valid)
            continue;
        if (cache != nullptr)
            cache->store (r.file, r.analysis);
        for (auto& song : show.songs)
            if (song.file == r.file)
                song.analysis = r.analysis;
    }
    // Saved once the queue is through (not after every song).
    if (cache != nullptr && analyser->pending() == 0)
        cache->save();
    showChanged();
}

void RKPlayProcessor::analyseNow()
{
    for (auto& song : show.songs)
    {
        if (song.analysis.valid || ! song.isPlayable())
            continue;
        if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (song.file)); reader != nullptr)
            song.analysis = PlayAnalysis::analyse (*reader);
        if (cache != nullptr && song.analysis.valid)
            cache->store (song.file, song.analysis);
    }
    if (cache != nullptr)
        cache->save();
    showChanged();
}

void RKPlayProcessor::setNote (const juce::String& text)
{
    note = text;
    ++noteVersion;
}

void RKPlayProcessor::showChanged()
{
    ++showVersion;
}

//==============================================================================
// State

juce::var RKPlayProcessor::getExtraState() const
{
    auto v = show.toVar();
    v.getDynamicObject()->setProperty ("current", currentIndex);
    return v;
}

void RKPlayProcessor::setExtraState (const juce::var& extra)
{
    // A show loading: the music stops (quickly) and the new list is checked.
    if (state != State::stopped)
        panic();
    show = {};
    if (extra.isObject())
        show.fromVar (extra, formats);
    currentIndex = juce::jlimit (0, juce::jmax (0, (int) show.songs.size() - 1), (int) extra.getProperty ("current", 0));
    if (const auto problems = show.numProblems())
        setNote (juce::String (problems) + (problems == 1 ? " file" : " files") + " missing or unreadable (marked in red)");
    attachAnalyses();
    showChanged();
}

juce::AudioProcessorEditor* RKPlayProcessor::createEditor()
{
    return new RKPlayEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RKPlayProcessor();
}
