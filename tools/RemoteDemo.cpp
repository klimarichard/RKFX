// Serves RKMix's remote pages (the monitor page, the console app) from a
// headless RKMix playing a test signal, for trying the pages in a browser
// without the app or a host. Its settings live in a temporary file (the
// computer's remote.json is never touched).
//
//   RKFXRemoteDemo_RKMix [state.json] [port] [pin]
//
// state.json: a mix (any RKMix preset). Default port 18080, PIN 1234.
// Each input gets a tone whose level drifts, so the meters move. Ctrl-C stops it.

#include "PluginProcessor.h"

#include <CoreFoundation/CoreFoundation.h>
#include <csignal>

namespace
{
    std::atomic<bool> quit { false };
    void onSignal (int) { quit = true; }

    class Player : public juce::Thread
    {
    public:
        explicit Player (juce::AudioProcessor& p) : juce::Thread ("demo audio"), processor (p) {}
        void run() override
        {
            constexpr int block = 256;
            constexpr double rate = 48000.0;
            juce::AudioBuffer<float> buffer (32, block);
            juce::MidiBuffer midi;
            double t = 0.0;
            std::array<double, 32> phase {};
            auto next = juce::Time::getMillisecondCounterHiRes();
            while (! threadShouldExit())
            {
                for (int ch = 0; ch < 32; ++ch)
                {
                    // A slow swell per input, some louder than others.
                    const auto level = 0.25 * (1.0 + std::sin (t * (0.3 + 0.07 * ch) + ch)) * (ch % 5 == 4 ? 0.1 : 1.0);
                    const auto freq = 110.0 * (1 + ch % 7);
                    auto* d = buffer.getWritePointer (ch);
                    for (int i = 0; i < block; ++i)
                    {
                        d[i] = (float) (level * std::sin (phase[(size_t) ch]));
                        phase[(size_t) ch] += 2.0 * juce::MathConstants<double>::pi * freq / rate;
                    }
                }
                processor.processBlock (buffer, midi);
                midi.clear();
                t += block / rate;
                next += 1000.0 * block / rate;
                const auto wait = next - juce::Time::getMillisecondCounterHiRes();
                if (wait > 1.0)
                    juce::Thread::sleep ((int) wait);
            }
        }
        juce::AudioProcessor& processor;
    };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::signal (SIGINT, onSignal);
    std::signal (SIGTERM, onSignal);

    auto mixer = std::make_unique<RKMixProcessor>();
    mixer->setPlayConfigDetails (32, 32, 48000.0, 256);
    mixer->prepareToPlay (48000.0, 256);
    if (argc > 1 && juce::String (argv[1]).isNotEmpty())
    {
        const auto json = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]).loadFileAsString();
        mixer->setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
    }

    auto& remote = mixer->getRemote();
    const auto settings = juce::File::createTempFile ("rkmix-remote-demo.json");
    remote.setSettingsFileForTesting (settings);
    remote.getConsole().setPin (argc > 3 ? argv[3] : "1234");
    remote.setPort (argc > 2 ? juce::String (argv[2]).getIntValue() : 18080);
    remote.setEnabled (true);
    remote.begin();
    std::printf ("%s\nconsole: http://localhost:%d/console  PIN %s\n", remote.getStatus().toRawUTF8(), remote.getPort(),
                 remote.getConsole().getPin().toRawUTF8());
    for (int bus = 0; bus < mix::kBuses; ++bus)
        if (mixer->getBusType (bus) == mix::BusType::aux)
            std::printf ("monitor %d: http://localhost:%d/m/%s\n", bus + 1, remote.getPort(), remote.getKey (bus).toRawUTF8());
    std::fflush (stdout);

    Player player (*mixer);
    player.startThread();
    // JUCE's messages and timers on macOS go through the main run loop.
    while (! quit)
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.1, true);
    player.stopThread (2000);
    settings.deleteFile();
    return 0;
}
