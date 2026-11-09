// Serves RKPlay's web remote from a headless RKPlay playing a made-up show
// (three playlists of short tones named like real songs, two pads), for
// trying the page in a browser without the app or a host. Its settings and
// analysis cache live in temporary files (the computer's remote.json is read
// but never changed, and no rig, session or history is written); the page's
// files are read from disk, so a change shows on reload.
//
//   RKFXRemoteDemo_RKPlay [port] [pin]
//
// Default port 18082, PIN 1234. With RKPLAY_DEMO_SNAPSHOT=<file.png>, a
// SIGUSR1 saves the window's Settings > Web remote page there (the devices
// connected at that moment). Ctrl-C stops it.

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <CoreFoundation/CoreFoundation.h>
#include <csignal>

namespace
{
    std::atomic<bool> quit { false }, snapshot { false };
    void onSignal (int) { quit = true; }
    void onSnapshot (int) { snapshot = true; }

    /** A 16-bit mono WAV of a quiet tone: small, so a show of them stays small. */
    juce::File writeSong (const juce::File& folder, const juce::String& name, double seconds, double hz)
    {
        constexpr double rate = 22050.0;
        auto file = folder.getChildFile (name + ".wav");
        const auto frames = (int) (seconds * rate);
        juce::AudioBuffer<float> buffer (1, frames);
        for (int i = 0; i < frames; ++i)
            buffer.setSample (0, i, (float) (0.1 * std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate)));
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> out (file.createOutputStream());
        if (auto writer = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (1).withBitsPerSample (16)))
            writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        return file;
    }

    class Audio : public juce::Thread
    {
    public:
        explicit Audio (juce::AudioProcessor& p) : juce::Thread ("demo audio"), processor (p) {}
        void run() override
        {
            constexpr int block = 256;
            constexpr double rate = 48000.0;
            juce::AudioBuffer<float> buffer (2, block);
            juce::MidiBuffer midi;
            auto next = juce::Time::getMillisecondCounterHiRes();
            while (! threadShouldExit())
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
                midi.clear();
                next += 1000.0 * block / rate;
                const auto wait = next - juce::Time::getMillisecondCounterHiRes();
                if (wait > 1.0)
                    juce::Thread::sleep ((int) wait);
            }
        }
        juce::AudioProcessor& processor;
    };

    void saveSettingsPage (RKPlayProcessor& player, const juce::File& png)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (player.createEditorIfNeeded());
        editor->setVisible (true);
        // Settings, then its Web remote page (as Snapshot.cpp clicks).
        for (const auto pos : { juce::Point<int> (1032, 308), juce::Point<int> (182, 704) })
            if (auto* target = editor->getComponentAt (pos))
            {
                const auto local = target->getLocalPoint (editor.get(), pos).toFloat();
                const auto now = juce::Time::getCurrentTime();
                const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), local,
                                          juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                          target, target, now, local, now, 1, false);
                target->mouseDown (e);
                target->mouseUp (e);
            }
        // Let its timer see the devices.
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.6, false);
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        png.deleteFile();
        juce::FileOutputStream stream (png);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        player.editorBeingDeleted (editor.get());
        std::printf ("wrote %s\n", png.getFullPathName().toRawUTF8());
        std::fflush (stdout);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::signal (SIGINT, onSignal);
    std::signal (SIGTERM, onSignal);
    std::signal (SIGUSR1, onSnapshot);

    const auto temp = juce::File::createTempFile ("rkplay-demo");
    const auto music = temp.getChildFile ("Music");
    music.createDirectory();

    // The hub first, on a temporary settings file: the player's beginRemote() shares it.
    juce::SharedResourcePointer<PlayRemoteHub> hub;
    hub->setSettingsFileForTesting (temp.getChildFile ("remote.json"));
    hub->setPin (argc > 2 ? argv[2] : "1234");
    hub->setPort (argc > 1 ? juce::String (argv[1]).getIntValue() : 18082);
    hub->setEnabled (true);

    auto player = std::make_unique<RKPlayProcessor>();
    player->useAnalysisCache (temp.getChildFile ("analysis.json"));
    player->setPlayConfigDetails (2, 2, 48000.0, 256);
    player->prepareToPlay (48000.0, 256);
    player->setLevelMatch (false);
    player->setSmartCrossfade (false);

    const char* reception[] = { "Norah Jones - Don't Know Why", "Michael Bubl\xc3\xa9 - Everything", "Ella Fitzgerald - Cheek to Cheek",
                                "Frank Sinatra - The Way You Look Tonight", "Diana Krall - The Look of Love", "Chet Baker - My Funny Valentine",
                                "Louis Armstrong - What a Wonderful World", "Nat King Cole - L-O-V-E" };
    const char* dinner[] = { "Ludovico Einaudi - Nuvole Bianche", "Yiruma - River Flows in You", "Erik Satie - Gymnop\xc3\xa9" "die No. 1",
                             "Karel Gott - Lady Carneval", "Hana Hegerov\xc3\xa1 - Lev\xc3\xa1 ruka", "Bed\xc5\x99ich Smetana - Vltava" };
    const char* party[] = { "ABBA - Dancing Queen", "Earth, Wind & Fire - September", "Queen - Don't Stop Me Now", "Bruno Mars - Uptown Funk",
                            "The Beatles - Twist and Shout", "Whitney Houston - I Wanna Dance with Somebody", "Kool & The Gang - Celebration",
                            "Daft Punk - Get Lucky", "Pharrell Williams - Happy", "Mark Ronson - Valerie", "Elvis Presley - Burning Love" };
    const auto add = [&] (int list, const char* const* names, int count, double hz)
    {
        juce::Array<juce::File> files;
        for (int i = 0; i < count; ++i)
            files.add (writeSong (music, juce::String (juce::CharPointer_UTF8 (names[i])), 25.0 + 3.0 * i, hz + 20.0 * i));
        player->addFiles (list, files);
    };
    player->renamePlaylist (0, "Reception");
    add (0, reception, 8, 220.0);
    add (player->addPlaylist ("Dinner"), dinner, 6, 330.0);
    add (player->addPlaylist ("Party"), party, 11, 440.0);
    player->loadPad (0, writeSong (music, "Fanfare", 3.0, 660.0));
    player->loadPad (1, writeSong (music, "Entrance", 5.0, 550.0));
    player->setPlaylistCrossfade (0, 3.0);

    player->beginRemote();
    std::printf ("%s\nhttp://localhost:%d/#pin=%s\n", hub->getStatus().toRawUTF8(), hub->getPort(), hub->getPin().toRawUTF8());
    std::fflush (stdout);

    Audio audio (*player);
    audio.startThread();
    player->play();
    const auto png = juce::SystemStats::getEnvironmentVariable ("RKPLAY_DEMO_SNAPSHOT", {});
    while (! quit)
    {
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.1, true);
        if (snapshot.exchange (false) && png.isNotEmpty())
            saveSettingsPage (*player, juce::File (png));
    }
    audio.stopThread (2000);
    player->releaseResources();
    player.reset();
    temp.deleteRecursively();
    return 0;
}
