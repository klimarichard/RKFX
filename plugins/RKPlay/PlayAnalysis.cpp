#include "PlayAnalysis.h"

#include "dsp/Loudness.h"

#include <cmath>

//==============================================================================
PlayAnalysis PlayAnalysis::analyse (juce::AudioFormatReader& reader, const std::atomic<bool>* cancel)
{
    PlayAnalysis a;
    const auto rate = reader.sampleRate;
    const auto length = reader.lengthInSamples;
    if (rate <= 0.0 || length <= 0)
        return a;

    // Measured as it plays: two channels (a mono file is on both sides).
    rkfx::LoudnessMeter meter;
    meter.prepare (rate, 2);
    const auto threshold = juce::Decibels::decibelsToGain (kSilenceDb);
    juce::int64 first = -1, last = -1;
    std::vector<float> overview ((size_t) kOverviewPoints, 0.0f);

    constexpr int chunk = 65536;
    juce::AudioBuffer<float> buffer (2, chunk);
    for (juce::int64 pos = 0; pos < length; pos += chunk)
    {
        if (cancel != nullptr && cancel->load())
            return {};
        const auto n = (int) juce::jmin<juce::int64> (chunk, length - pos);
        reader.read (&buffer, 0, n, pos, true, true);
        meter.process (buffer.getArrayOfReadPointers(), n);

        const auto* l = buffer.getReadPointer (0);
        const auto* r = buffer.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const auto peak = juce::jmax (std::abs (l[i]), std::abs (r[i]));
            const auto index = pos + i;
            if (peak > threshold)
            {
                if (first < 0)
                    first = index;
                last = index;
            }
            auto& point = overview[(size_t) juce::jmin<juce::int64> (kOverviewPoints - 1, index * kOverviewPoints / length)];
            point = juce::jmax (point, peak);
        }
    }

    a.lengthSeconds = (double) length / rate;
    a.loudness = meter.getIntegrated();
    a.truePeakDb = juce::Decibels::gainToDecibels (meter.getTruePeak(), -200.0f);
    a.samplePeakDb = juce::Decibels::gainToDecibels (meter.getSamplePeak(), -200.0f);

    // Where the music is: a little before it first passes -50 dBFS, a little after it last does.
    if (first >= 0)
    {
        a.musicStart = juce::jmax (0.0, (double) first / rate - 0.01);
        a.musicEnd = juce::jmin (a.lengthSeconds, (double) last / rate + 0.05);
    }
    else
    {
        a.musicEnd = a.lengthSeconds;
    }

    // The fade-out point: after it, every 400 ms block stays 20 LU under the song.
    a.fadeOutPoint = a.musicEnd;
    const auto blocks = meter.getMomentary();
    for (int b = (int) blocks.size() - 1; b >= 0; --b)
        if (blocks[(size_t) b] >= a.loudness - kFadeOutBelowLu)
        {
            const auto halfway = a.musicStart + 0.5 * (a.musicEnd - a.musicStart);
            a.fadeOutPoint = juce::jlimit (halfway, a.musicEnd, b * 0.1 + 0.4);
            break;
        }

    a.overview.reserve ((size_t) kOverviewPoints);
    for (const auto p : overview)
        a.overview.push_back ((juce::uint8) juce::jlimit (0, 255, (int) std::lround (juce::jmin (1.0f, p) * 255.0f)));
    a.valid = true;
    return a;
}

juce::var PlayAnalysis::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("loudness", loudness);
    o->setProperty ("truePeak", truePeakDb);
    o->setProperty ("samplePeak", samplePeakDb);
    o->setProperty ("length", lengthSeconds);
    o->setProperty ("start", musicStart);
    o->setProperty ("end", musicEnd);
    o->setProperty ("fadeOut", fadeOutPoint);
    o->setProperty ("overview", juce::Base64::toBase64 (overview.data(), overview.size()));
    return juce::var (o);
}

PlayAnalysis PlayAnalysis::fromVar (const juce::var& v)
{
    PlayAnalysis a;
    if (! v.isObject())
        return a;
    a.loudness = v.getProperty ("loudness", -200.0);
    a.truePeakDb = (float) (double) v.getProperty ("truePeak", -200.0);
    a.samplePeakDb = (float) (double) v.getProperty ("samplePeak", -200.0);
    a.lengthSeconds = v.getProperty ("length", 0.0);
    a.musicStart = v.getProperty ("start", 0.0);
    a.musicEnd = v.getProperty ("end", a.lengthSeconds);
    a.fadeOutPoint = v.getProperty ("fadeOut", a.musicEnd);
    juce::MemoryOutputStream decoded;
    if (juce::Base64::convertFromBase64 (decoded, v["overview"].toString()))
    {
        const auto* bytes = static_cast<const juce::uint8*> (decoded.getData());
        a.overview.assign (bytes, bytes + decoded.getDataSize());
    }
    a.valid = a.lengthSeconds > 0.0;
    return a;
}

//==============================================================================
PlayLevel PlayLevel::plan (const PlayAnalysis& a, const Settings& s, double songGainDb)
{
    PlayLevel level;
    if (! s.match || ! a.valid || a.loudness < -70.0)
    {
        level.gainDb = songGainDb;
        return level;
    }

    // Pure gain to the target, as far as the peak ceiling allows; then lift
    // (limited) by up to liftDb more. The song's own gain goes on top.
    const auto needed = s.targetLufs - a.loudness;
    const auto room = s.peakCeilingDb - a.truePeakDb;
    auto gain = needed;
    if (needed > room)
        gain = room + juce::jmin (needed - room, juce::jmax (0.0, s.liftDb));
    gain += songGainDb;

    // Within 0.1 dB of nothing is nothing: a file already normalised to the
    // target (the user's archive measures -17.95 to -18.05) plays bit-exact.
    level.gainDb = std::abs (gain) < kOnTargetDb ? 0.0 : gain;
    level.limitDb = juce::jmax (0.0, level.gainDb - room);
    if (level.limitDb < 0.05)
        level.limitDb = 0.0;
    return level;
}

//==============================================================================
PlayAnalysisCache::PlayAnalysisCache (juce::File f) : file (std::move (f))
{
    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (const auto* list = parsed["songs"].getDynamicObject())
        for (const auto& p : list->getProperties())
            entries[p.name.toString()] = p.value;
}

juce::File PlayAnalysisCache::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support/RKFX/RKPlay/analysis.json");
}

juce::String PlayAnalysisCache::keyFor (const juce::File& f)
{
    return f.getFullPathName() + "|" + juce::String (f.getSize()) + "|" + juce::String (f.getLastModificationTime().toMilliseconds());
}

bool PlayAnalysisCache::find (const juce::File& f, PlayAnalysis& out) const
{
    const auto it = entries.find (keyFor (f));
    if (it == entries.end())
        return false;
    out = PlayAnalysis::fromVar (it->second);
    return out.valid;
}

void PlayAnalysisCache::store (const juce::File& f, const PlayAnalysis& a)
{
    // A file measured again replaces its old entries.
    const auto path = f.getFullPathName() + "|";
    for (auto it = entries.begin(); it != entries.end();)
        it = it->first.startsWith (path) ? entries.erase (it) : std::next (it);
    entries[keyFor (f)] = a.toVar();
    dirty = true;
}

void PlayAnalysisCache::save()
{
    if (! dirty)
        return;
    auto* songs = new juce::DynamicObject();
    for (const auto& [key, value] : entries)
        songs->setProperty (key, value);
    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("songs", juce::var (songs));
    file.getParentDirectory().createDirectory();
    if (file.replaceWithText (juce::JSON::toString (juce::var (root), true)))
        dirty = false;
}

//==============================================================================
PlayAnalyser::PlayAnalyser() : juce::Thread ("RKPlay analysis")
{
    formats.registerBasicFormats();
    startThread (juce::Thread::Priority::background);
}

PlayAnalyser::~PlayAnalyser()
{
    cancelFlag = true;
    signalThreadShouldExit();
    wake.signal();
    stopThread (4000);
}

void PlayAnalyser::add (const juce::File& f)
{
    {
        const juce::ScopedLock sl (lock);
        if (f == working || std::find (queue.begin(), queue.end(), f) != queue.end())
            return;
        queue.push_back (f);
    }
    wake.signal();
}

int PlayAnalyser::pending() const
{
    const juce::ScopedLock sl (lock);
    return (int) queue.size() + (working != juce::File() ? 1 : 0);
}

std::vector<PlayAnalyser::Result> PlayAnalyser::takeResults()
{
    const juce::ScopedLock sl (lock);
    return std::exchange (results, {});
}

void PlayAnalyser::run()
{
    while (! threadShouldExit())
    {
        juce::File next;
        {
            const juce::ScopedLock sl (lock);
            if (! queue.empty())
            {
                next = queue.front();
                queue.pop_front();
                working = next;
            }
        }
        if (next == juce::File())
        {
            wake.wait (500);
            continue;
        }

        Result result { next, {} };
        if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (next)); reader != nullptr)
            result.analysis = PlayAnalysis::analyse (*reader, &cancelFlag);

        const juce::ScopedLock sl (lock);
        working = juce::File();
        if (! cancelFlag.load())
            results.push_back (std::move (result));
    }
}
