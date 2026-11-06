// Serves RKBoard's remote page from two headless RKBoards (a factory board
// each, the first behaving as the app: tuner and tap), for trying the page
// in a browser without the app or a host. Its settings live in a temporary
// file (the computer's remote.json is never touched); the page's files are
// read from disk, so a change shows on reload.
//
//   RKFXRemoteDemo_RKBoard [port] [pin]
//
// Default port 18081, PIN 1234. A tone plays into the boards (the tuner
// shows an A a little sharp). Ctrl-C stops it.

#include "BoardProcessor.h"

#include <CoreFoundation/CoreFoundation.h>
#include <csignal>

namespace
{
    std::atomic<bool> quit { false };
    void onSignal (int) { quit = true; }

    class Player : public juce::Thread
    {
    public:
        explicit Player (std::vector<juce::AudioProcessor*> p) : juce::Thread ("demo audio"), processors (std::move (p)) {}
        void run() override
        {
            constexpr int block = 256;
            constexpr double rate = 48000.0;
            juce::AudioBuffer<float> buffer (2, block);
            juce::MidiBuffer midi;
            std::vector<double> phases (processors.size(), 0.0);
            auto next = juce::Time::getMillisecondCounterHiRes();
            while (! threadShouldExit())
            {
                for (size_t p = 0; p < processors.size(); ++p)
                {
                    auto& phase = phases[p];
                    for (int i = 0; i < block; ++i)
                    {
                        const auto v = (float) (0.3 * std::sin (phase));
                        buffer.setSample (0, i, v);
                        buffer.setSample (1, i, v);
                        phase += 2.0 * juce::MathConstants<double>::pi * 110.6 / rate;
                    }
                    processors[p]->processBlock (buffer, midi);
                    midi.clear();
                }
                next += 1000.0 * block / rate;
                const auto wait = next - juce::Time::getMillisecondCounterHiRes();
                if (wait > 1.0)
                    juce::Thread::sleep ((int) wait);
            }
        }
        std::vector<juce::AudioProcessor*> processors;
    };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::signal (SIGINT, onSignal);
    std::signal (SIGTERM, onSignal);

    const auto rig = juce::File::createTempFile ("rkboard-demo-rig");
    rig.createDirectory();
    auto first = std::make_unique<rkboard::BoardProcessor>();
    auto second = std::make_unique<rkboard::BoardProcessor>();
    first->setStandaloneForTesting (rig);
    for (auto* board : { first.get(), second.get() })
    {
        board->setPlayConfigDetails (2, 2, 48000.0, 256);
        board->prepareToPlay (48000.0, 256);
    }
    first->loadFactoryPreset (5);    // Scenes Starter
    second->loadFactoryPreset (3);   // Bass Split
    first->setRemoteName ("Guitar");
    second->setRemoteName ("Bass");
    first->setSceneName (0, "Intro");
    first->setSceneName (1, "Verse");
    first->setSceneName (2, "Chorus");
    first->setSceneName (3, "Solo");
    first->setTunerActive (true);
    if (const auto chain = first->getChain(); chain.size() >= 3)
    {
        first->assign (0, chain.back(), "mix");      // an expression pedal on the last pedal's mix
        first->assign (1, chain[1], "bypass");       // a footswitch
    }

    rkboard::BoardRemoteHub hub;
    const auto settings = juce::File::createTempFile ("rkboard-remote-demo.json");
    hub.setSettingsFileForTesting (settings);
    hub.setPin (argc > 2 ? argv[2] : "1234");
    hub.setPort (argc > 1 ? juce::String (argv[1]).getIntValue() : 18081);
    hub.setEnabled (true);
    hub.addBoard (*first, true);
    hub.addBoard (*second, true);
    std::printf ("%s\nhttp://localhost:%d/#pin=%s\n", hub.getStatus().toRawUTF8(), hub.getPort(), hub.getPin().toRawUTF8());
    std::fflush (stdout);

    Player player ({ first.get(), second.get() });
    player.startThread();
    while (! quit)
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.1, true);
    player.stopThread (2000);
    hub.removeBoard (*first);
    hub.removeBoard (*second);
    settings.deleteFile();
    rig.deleteRecursively();
    return 0;
}
