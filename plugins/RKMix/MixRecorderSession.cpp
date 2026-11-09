#include "MixRecorderSession.h"

#include "PluginProcessor.h"
#include "plugin/PresetLibrary.h"

namespace
{
    juce::String two (int n) { return juce::String (n).paddedLeft ('0', 2); }
    juce::String legal (const juce::String& s) { return juce::File::createLegalFileName (s).trim(); }
}

MixRecorderSession::MixRecorderSession (RKMixProcessor& p) : mixer (p)
{
    settingsFile = rkfx::PresetLibrary::defaultSettingsFile().getParentDirectory().getChildFile ("RKMix").getChildFile ("recorder.json");
    rootFolder = defaultRootFolder();
    loadSettings();
    startTimerHz (10);
}

MixRecorderSession::~MixRecorderSession()
{
    stopTimer();
    recorder.stop();
}

juce::File MixRecorderSession::defaultRootFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("RKMusic Plugins/RKFX/Recordings");
}

//==============================================================================
void MixRecorderSession::setSettingsFileForTesting (const juce::File& f)
{
    settingsFile = f;
    rootFolder = defaultRootFolder();
    asFloat = pairsAsMono = false;
    loadSettings();
}

void MixRecorderSession::loadSettings()
{
    const auto v = juce::JSON::parse (settingsFile);
    if (! v.isObject())
        return;
    if (const auto path = v["folder"].toString(); juce::File::isAbsolutePath (path))
        rootFolder = juce::File (path);
    asFloat = (bool) v.getProperty ("float", false);
    pairsAsMono = (bool) v.getProperty ("pairsAsMono", false);
}

void MixRecorderSession::saveSettings() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("folder", rootFolder.getFullPathName());
    o->setProperty ("float", asFloat);
    o->setProperty ("pairsAsMono", pairsAsMono);
    settingsFile.getParentDirectory().createDirectory();
    settingsFile.replaceWithText (juce::JSON::toString (juce::var (o)));
}

void MixRecorderSession::setRootFolder (const juce::File& f) { rootFolder = f; saveSettings(); }
void MixRecorderSession::setFloat (bool on)                  { asFloat = on; saveSettings(); }
void MixRecorderSession::setPairsAsMono (bool on)            { pairsAsMono = on; saveSettings(); }

//==============================================================================
MixRecorder::Take MixRecorderSession::makeTake() const
{
    using Source = MixRecorder::Source;
    const auto& s = mixer.getSettings();
    MixRecorder::Take t;
    t.sampleRate = mixer.getSampleRate() > 0.0 ? mixer.getSampleRate() : 48000.0;
    t.asFloat = asFloat;

    // The files are the inputs (as they arrive), named after the channel each
    // feeds (the input patch: its first channel, a pair's name); an input no
    // channel takes keeps its number.
    // Inputs n and n + 1 are a stereo pair when a linked pair takes them, left and right: its leader, or -1.
    const auto pairOf = [this] (int in)
    {
        if (in % 2 != 0 || in + 1 >= mix::kChannels)
            return -1;
        for (int pair = 0; pair < mix::kPairs; ++pair)
            if (mixer.isLinked (pair) && mixer.getChannelInput (pair * 2) == in && mixer.getChannelInput (pair * 2 + 1) == in + 1)
                return pair * 2;
        return -1;
    };
    const auto pairedInputs = [&] (int in) { return pairOf (in) >= 0; };
    // An input split onto several channels takes the first named one's name (a pair's first).
    const auto name = [&] (int in)
    {
        const auto leader = [this] (int ch) { return ch % 2 == 1 && mixer.isLinked (ch / 2) ? ch - 1 : ch; };
        juce::Array<int> channels;
        if (const auto pair = pairOf (in - in % 2); pair >= 0)
            channels.add (pair);
        for (const auto ch : mixer.channelsOfInput (in))
            channels.addIfNotAlreadyThere (leader (ch));
        if (channels.isEmpty())
            return legal ("In " + juce::String (in + 1));
        for (const auto ch : channels)
            if (mixer.getChannelName (ch).isNotEmpty())
                return legal (mixer.getChannelName (ch));
        return legal ("Ch " + juce::String (channels[0] + 1));
    };
    const auto inPairedInputs = [&] (int in) { return pairedInputs (in - in % 2); };

    // The inputs: a linked pair's two as one stereo file (or two mono ones), the rest mono.
    for (int ch = 0; ch < mix::kChannels; ++ch)
    {
        const auto pair = pairedInputs (ch);
        if (pair && ! pairsAsMono)
        {
            const auto left = s.flag (mix::recordInputId (ch)), right = s.flag (mix::recordInputId (ch + 1));
            if (left && right)
                t.tracks.push_back ({ two (ch + 1) + "-" + two (ch + 2) + " " + name (ch) + ".wav",
                                      { { Source::input, ch, 0 }, { Source::input, ch + 1, 0 } } });
            else if (left || right)
                t.tracks.push_back ({ two (ch + (left ? 1 : 2)) + " " + name (ch) + (left ? " L" : " R") + ".wav",
                                      { { Source::input, left ? ch : ch + 1, 0 } } });
            ++ch;
            continue;
        }
        if (! s.flag (mix::recordInputId (ch)))
            continue;
        const auto inPair = inPairedInputs (ch);
        const auto label = inPair ? name (ch - ch % 2) + (ch % 2 == 0 ? " L" : " R") : name (ch);
        t.tracks.push_back ({ two (ch + 1) + " " + label + ".wav", { { Source::input, ch, 0 } } });
    }

    if (s.flag ("rec_main"))
        t.tracks.push_back ({ "Main.wav", { { Source::main, 0, 0 }, { Source::main, 0, 1 } } });

    for (int bus = 0; bus < mix::kBuses; ++bus)
        if (s.flag (mix::recordBusId (bus)) && mixer.getBusType (bus) == mix::BusType::aux)
        {
            const auto busName = mixer.getBusName (bus);
            MixRecorder::Track track { "Aux " + two (bus + 1) + " " + legal (busName.isNotEmpty() ? busName : "Bus " + juce::String (bus + 1)) + ".wav", {} };
            track.sources.push_back ({ Source::bus, bus, 0 });
            if (mixer.isBusStereo (bus))
                track.sources.push_back ({ Source::bus, bus, 1 });
            t.tracks.push_back (track);
        }

    // session.json: the mix, the names and colours as recording started.
    auto* info = new juce::DynamicObject();
    info->setProperty ("mix", mixer.getPresetName());
    juce::Array<juce::var> names, colours;
    for (int ch = 0; ch < mix::kChannels; ++ch)
    {
        names.add (mixer.getChannelName (ch));
        colours.add (mixer.getChannelColour (ch));
    }
    info->setProperty ("names", names);
    info->setProperty ("colours", colours);
    juce::Array<juce::var> patch;   // each channel's Source as recording started: 0 Off, n In n
    for (int ch = 0; ch < mix::kChannels; ++ch)
        patch.add (mixer.getChannelInput (ch) + 1);
    info->setProperty ("patch", patch);
    t.info = juce::var (info);

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M");
    t.folder = rootFolder.getChildFile (legal (stamp + " " + mixer.getPresetName())).getNonexistentSibling (true);
    return t;
}

int MixRecorderSession::countChannels() const
{
    int n = 0;
    for (const auto& track : makeTake().tracks)
        n += (int) track.sources.size();
    return n;
}

double MixRecorderSession::getFreeBytes() const
{
    auto f = rootFolder;
    while (! f.exists() && f.getParentDirectory() != f)
        f = f.getParentDirectory();
    return (double) f.getBytesFreeOnVolume();
}

double MixRecorderSession::getSecondsLeft() const
{
    const auto rate = mixer.getSampleRate() > 0.0 ? mixer.getSampleRate() : 48000.0;
    const auto perSecond = MixRecorder::bytesPerSecondFor (juce::jmax (1, countChannels()), rate, asFloat);
    return getFreeBytes() / perSecond;
}

double MixRecorderSession::getElapsedSeconds() const noexcept
{
    return (double) recorder.getRecordedSamples() / juce::jmax (1.0, recorder.getSampleRate());
}

//==============================================================================
juce::Result MixRecorderSession::start()
{
    if (isRecording())
        return juce::Result::ok();

    const auto take = makeTake();
    auto result = take.tracks.empty() ? juce::Result::fail ("nothing chosen to record")
                : getSecondsLeft() < kMinStartMinutes * 60.0 ? juce::Result::fail ("less than 10 minutes of disk space left")
                : recorder.start (take);
    if (result.wasOk())
    {
        status = {};
        markerCount = 0;
        for (int pair = 0; pair < mix::kPairs; ++pair)
            takeLinks[(size_t) pair] = mixer.isLinked (pair);
    }
    else
        status = "Not started: " + result.getErrorMessage();
    setRecordParameter (isRecording());
    return result;
}

void MixRecorderSession::stop()
{
    recorder.stop();
    setRecordParameter (false);
}

void MixRecorderSession::addMarker (const juce::String& label)
{
    if (! isRecording())
        return;
    recorder.addMarker (label.isNotEmpty() ? label : "Marker " + juce::String (++markerCount));
}

void MixRecorderSession::sceneRecalled (int scene)
{
    addMarker ("Scene " + juce::String (scene + 1));
}

void MixRecorderSession::mixLoaded()
{
    if (! isRecording())
        return;

    // The links may have changed: a new take, from the next block, with the new mix's layout.
    recorder.addNote ("split: a mix was loaded or the session restored; the recording goes on in the next take");
    const auto previous = recorder.getFolder();
    auto take = makeTake();
    if (auto* info = take.info.getDynamicObject())
        info->setProperty ("notes", juce::Array<juce::var> { "split: continues " + previous.getFileName() });
    if (const auto r = recorder.start (take); r.failed())
    {
        recorder.stop();
        status = "Stopped: " + r.getErrorMessage();
    }
    for (int pair = 0; pair < mix::kPairs; ++pair)
        takeLinks[(size_t) pair] = mixer.isLinked (pair);
    setRecordParameter (isRecording());
}

void MixRecorderSession::sampleRateChanged()
{
    if (isRecording())
    {
        recorder.addNote ("stopped: the sample rate changed");
        stop();
        status = "Stopped: the sample rate changed";
    }
}

void MixRecorderSession::setRecordParameter (bool on)
{
    wantRecord = on ? 1 : 0;
    if (auto* p = mixer.findParam ("rec"); p != nullptr && (p->getValue() >= 0.5f) != on)
        p->setValueNotifyingHost (on ? 1.0f : 0.0f);
    wantRecord = -1;
}

void MixRecorderSession::timerCallback()
{
    // The Record parameter (a Gig Performer button): on starts, off stops.
    if (const auto want = wantRecord.exchange (-1); want >= 0 && (want == 1) != isRecording())
    {
        if (want == 1)
            start();
        else
            stop();
    }
    for (auto n = markerPresses.exchange (0); n > 0; --n)
        addMarker();

    if (! isRecording())
        return;

    // The disk is about to fill: stop cleanly (the files closed).
    if (recorder.isDiskNearlyFull())
    {
        recorder.addNote ("stopped: under a minute of disk space left");
        stop();
        status = "Stopped: the disk is nearly full";
        return;
    }

    // Links never change while recording (the files match the channels as they were).
    for (int pair = 0; pair < mix::kPairs; ++pair)
        if (mixer.isLinked (pair) != takeLinks[(size_t) pair])
            mixer.getSettings().param (mix::linkId (pair)).setValueNotifyingHost (takeLinks[(size_t) pair] ? 1.0f : 0.0f);
}

//==============================================================================
void MixRecorderSession::beginBlock (const float* const* inputs, int numSamples) noexcept
{
    active = recorder.isActive();
    if (! active)
        return;
    recorder.beginBlock (numSamples);
    for (int ch = 0; ch < mix::kChannels; ++ch)
        recorder.pushInput (ch, inputs[ch]);
}

void MixRecorderSession::endBlock() noexcept
{
    if (active)
        recorder.endBlock();
    active = false;
}

void MixRecorderSession::main (const float* left, const float* right, int) noexcept
{
    if (active)
        recorder.pushMain (left, right);
}

void MixRecorderSession::bus (int bus, const float* left, const float* right, int) noexcept
{
    if (active)
        recorder.pushBus (bus, left, right);
}
