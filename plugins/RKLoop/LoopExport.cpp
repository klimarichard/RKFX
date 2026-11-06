#include "LoopExport.h"

namespace LoopExport
{
    juce::File loopsFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("RKMusic Plugins/RKFX/Loops");
    }

    juce::String stamp (juce::Time t)
    {
        return t.formatted ("%Y-%m-%d %H-%M");
    }

    juce::String bpmSuffix (std::optional<double> bpm)
    {
        if (! bpm.has_value() || ! (*bpm > 0.0))
            return {};
        const auto tenths = juce::roundToInt (*bpm * 10.0);
        const auto text = tenths % 10 == 0 ? juce::String (tenths / 10) : juce::String (tenths / 10.0, 1);
        return " " + text + " BPM";
    }

    static juce::String loopLabel (int loop, bool layer)
    {
        return "Loop " + juce::String (loop + 1) + (layer ? " Layer" : " Part");
    }

    juce::String singleFileName (juce::Time t, int loop, bool layer, std::optional<double> bpm)
    {
        return "RKLoop " + stamp (t) + " " + loopLabel (loop, layer) + bpmSuffix (bpm) + ".wav";
    }

    juce::String folderName (juce::Time t)
    {
        return "RKLoop " + stamp (t);
    }

    juce::String fileInFolderName (int loop, bool layer)
    {
        return loopLabel (loop, layer) + ".wav";
    }

    juce::Result writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate, Format format)
    {
        if (audio.getNumSamples() <= 0 || audio.getNumChannels() <= 0)
            return juce::Result::fail ("the loop is empty");

        if (! file.getParentDirectory().createDirectory())
            return juce::Result::fail ("can't create the folder " + file.getParentDirectory().getFullPathName());

        // Write next to it, then move into place: a failed write never leaves half a file.
        juce::TemporaryFile temp (file);
        std::unique_ptr<juce::OutputStream> stream = temp.getFile().createOutputStream();
        if (stream == nullptr)
            return juce::Result::fail ("can't write to " + file.getParentDirectory().getFullPathName());

        // One forward loop over the whole file, for samplers (and Logic).
        const auto last = juce::String (audio.getNumSamples() - 1);
        const std::unordered_map<juce::String, juce::String> metadata {
            { "SamplePeriod",    juce::String ((juce::int64) std::llround (1.0e9 / sampleRate)) },
            { "MidiUnityNote",   "60" },
            { "NumSampleLoops",  "1" },
            { "Loop0Identifier", "0" },
            { "Loop0Type",       "0" },
            { "Loop0Start",      "0" },
            { "Loop0End",        last },
            { "Loop0Fraction",   "0" },
            { "Loop0PlayCount",  "0" },
        };

        const auto isFloat = format == Format::float32;
        const auto options = juce::AudioFormatWriterOptions{}
                                 .withSampleRate (sampleRate)
                                 .withNumChannels (audio.getNumChannels())
                                 .withBitsPerSample (isFloat ? 32 : 24)
                                 .withSampleFormat (isFloat ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                            : juce::AudioFormatWriterOptions::SampleFormat::integral)
                                 .withMetadataValues (metadata);

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, options);
        if (writer == nullptr)
            return juce::Result::fail ("can't create a WAV file");

        if (! writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
            return juce::Result::fail ("couldn't write " + file.getFileName() + " (is the disk full?)");
        if (! writer->flush())
            return juce::Result::fail ("couldn't write " + file.getFileName() + " (is the disk full?)");
        writer.reset();

        if (! temp.overwriteTargetFileWithTemporary())
            return juce::Result::fail ("couldn't save " + file.getFullPathName());
        return juce::Result::ok();
    }

    juce::String writeTakes (std::vector<Take>& takes, double sampleRate, Format format, const juce::StringArray& skipped)
    {
        for (auto& take : takes)
            if (const auto r = writeWav (take.file, take.audio, sampleRate, format); r.failed())
                return "Not saved: " + r.getErrorMessage();

        juce::String text;
        if (takes.size() == 1)
            text = "Saved Loop " + juce::String (takes.front().loop + 1);
        else if (! takes.empty())
            text = "Saved " + juce::String ((int) takes.size()) + " loops";
        else
            text = "Nothing saved";

        if (! skipped.isEmpty())
            text << " (" << skipped.joinIntoString (", ") << (skipped.size() == 1 ? " was" : " were") << " busy: skipped)";
        return text;
    }
}

//==============================================================================
namespace
{
    std::atomic<bool> settingsFileOn { true };

    std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        if (! settingsFileOn)
            return {};
        juce::PropertiesFile::Options o;
        o.applicationName = "RKLoop";
        o.filenameSuffix = ".settings";
        o.folderName = "RKMusic";
        o.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (o);
    }

    constexpr int kStatusSeconds = 5;
}

void LoopExporter::useSettingsFile (bool on) noexcept
{
    settingsFileOn = on;
}

LoopExporter::LoopExporter (const LoopEngine& e, Context c)
    : engine (e), context (std::move (c)), settings (openSettings())
{
    if (settings != nullptr)
        format = settings->getValue ("exportFormat") == "float32" ? LoopExport::Format::float32 : LoopExport::Format::int24;
    startTimer (100);
}

LoopExporter::~LoopExporter()
{
    stopTimer();
    waitForWriter();                   // the files being written are finished
}

void LoopExporter::waitForWriter() const
{
    while (pool.getNumJobs() > 0)
        juce::Thread::sleep (2);
}

std::vector<int> LoopExporter::loopsWithAudio() const
{
    std::vector<int> result;
    for (int i = 0; i < LoopEngine::kLoops; ++i)
    {
        const auto s = engine.getState (i);
        if (s != LoopEngine::State::empty && s != LoopEngine::State::recording && engine.getLength (i) > 0)
            result.push_back (i);
    }
    return result;
}

bool LoopExporter::isRunning() const noexcept
{
    return request.has_value() || writing;
}

void LoopExporter::exportLoop (int loop, const juce::File& file)
{
    std::vector<LoopExport::Take> takes (1);
    takes[0].loop = loop;
    takes[0].file = file.withFileExtension ("wav");
    start (std::move (takes));
}

void LoopExporter::exportAll (const juce::File& parentFolder)
{
    const auto folder = parentFolder.getChildFile (LoopExport::folderName (juce::Time::getCurrentTime())).getNonexistentSibling (true);
    std::vector<LoopExport::Take> takes;
    for (const auto i : loopsWithAudio())
    {
        LoopExport::Take t;
        t.loop = i;
        t.file = folder.getChildFile (LoopExport::fileInFolderName (i, context.isLayer (i)));
        takes.push_back (std::move (t));
    }
    if (takes.empty())
    {
        setStatus ("Nothing to save: every loop is empty", true);
        return;
    }
    start (std::move (takes));
}

void LoopExporter::quickSave()
{
    exportAll (LoopExport::loopsFolder());
}

void LoopExporter::start (std::vector<LoopExport::Take> takes)
{
    if (isRunning())
    {
        setStatus ("Still saving the last export", true);
        return;
    }

    Request r;
    r.wanted = std::move (takes);
    r.copied.assign (r.wanted.size(), false);
    r.started = juce::Time::getCurrentTime();
    r.sampleRate = context.sampleRate();
    request = std::move (r);
    setStatus ("Saving...", false);
    timerCallback();                   // copy what can be copied now
}

void LoopExporter::timerCallback()
{
    // The Save Loops footswitch.
    if (quickSaveRequests.exchange (0) > 0)
        quickSave();

    // The writer has finished.
    {
        const juce::ScopedLock sl (resultLock);
        if (hasWrittenResult)
        {
            hasWrittenResult = false;
            setStatus (writtenResult, writtenResult.startsWith ("Not saved"));
        }
    }

    if (! request.has_value())
        return;

    // Copy the loops that aren't busy; retry the others.
    auto& r = *request;
    auto done = true;
    for (size_t k = 0; k < r.wanted.size(); ++k)
    {
        if (r.copied[k])
            continue;
        if (engine.copyLoop (r.wanted[k].loop, r.wanted[k].audio, context.numChannels()))
            r.copied[k] = true;
        else
            done = false;
    }

    const auto timedOut = (juce::Time::getCurrentTime() - r.started).inSeconds() >= retrySeconds;
    if (! done && ! timedOut)
        return;

    std::vector<LoopExport::Take> takes;
    juce::StringArray skipped;
    for (size_t k = 0; k < r.wanted.size(); ++k)
    {
        if (r.copied[k])
            takes.push_back (std::move (r.wanted[k]));
        else if (engine.getState (r.wanted[k].loop) != LoopEngine::State::empty)
            skipped.add ("Loop " + juce::String (r.wanted[k].loop + 1));
    }
    const auto sampleRate = r.sampleRate;
    request.reset();

    if (takes.empty())
    {
        setStatus (skipped.isEmpty() ? juce::String ("Nothing saved: the loop is empty")
                                     : "Not saved: " + skipped.joinIntoString (", ") + " busy (finish the overdub first)", true);
        return;
    }

    // The files are written on the background thread.
    writing = true;
    const auto fmt = format;
    pool.addJob ([this, takes = std::move (takes), skipped, sampleRate, fmt]() mutable
    {
        const auto text = LoopExport::writeTakes (takes, sampleRate, fmt, skipped);
        const juce::ScopedLock sl (resultLock);
        writtenResult = text;
        hasWrittenResult = true;
        writing = false;
    });
}

void LoopExporter::setStatus (const juce::String& text, bool error)
{
    status = text;
    statusError = error;
    statusTime = juce::Time::getCurrentTime();
}

juce::String LoopExporter::getStatus() const
{
    if (isRunning())
        return "Saving...";
    if (status.isEmpty() || (juce::Time::getCurrentTime() - statusTime).inSeconds() > kStatusSeconds)
        return {};
    return status;
}

void LoopExporter::setFormat (LoopExport::Format f)
{
    format = f;
    if (settings != nullptr)
    {
        settings->setValue ("exportFormat", f == LoopExport::Format::float32 ? "float32" : "int24");
        settings->saveIfNeeded();
    }
}

juce::File LoopExporter::getLastFolder() const
{
    if (settings != nullptr)
        if (const auto path = settings->getValue ("exportFolder"); juce::File::isAbsolutePath (path) && juce::File (path).isDirectory())
            return juce::File (path);
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
}

void LoopExporter::setLastFolder (const juce::File& folder)
{
    if (settings != nullptr)
    {
        settings->setValue ("exportFolder", folder.getFullPathName());
        settings->saveIfNeeded();
    }
}
