#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MixConsole.h"
#include "MixLayout.h"
#include "MixWebServer.h"

#include <array>
#include <atomic>
#include <memory>

class RKMixProcessor;

/** The monitor remote: each musician's phone (or tablet) sets their own Aux
    mix in a web page RKMix serves on the local network, over a WebSocket.

    Each Aux bus has its own link (http://<mac>.local:8080/m/<key>), shown as
    a QR code on the Remote page. The key is made from a secret kept with the
    computer (Application Support/RKFX/RKMix/remote.json), so the links stay
    the same from mix to mix and a phone's home-screen icon keeps working; New
    links makes new keys (the old ones stop working). A page can only change
    its own bus: the sends into it (levels, and pans in an Own bus), the
    returns that feed it (FX and Group buses), and its fader and mute.

    The page gets the bus's mix when it changes and meters 15 times a second.
    The page is a web app: Add to Home Screen in Safari opens it full screen.

    The same server serves the console remote (MixConsole): the whole console
    on an iPad at /console, behind a PIN.

    Settings (on, port, links by IP address) are the computer's, shared by
    the plugin and the standalone app. In Gig Performer the first RKMix to
    start takes the port; another says the port is in use. */
class MixRemote : private juce::Timer
{
public:
    explicit MixRemote (RKMixProcessor&);
    ~MixRemote() override;

    /** Starts serving if it's on (the plugin and the app; tests and tools never call it). */
    void begin();

    bool isEnabled() const noexcept { return enabled; }
    void setEnabled (bool);
    int getPort() const noexcept { return port; }
    void setPort (int);
    bool linksUseIpAddress() const noexcept { return useIp; }
    void setLinksUseIpAddress (bool);
    /** Every bus gets a new link; phones with the old one must scan again. */
    void newLinks();

    /** "Listening at ...", "Port 8080 is in use", "Off". */
    juce::String getStatus() const;
    bool isListening() const noexcept { return server.isRunning(); }

    /** The address of the page for Aux bus @p bus (with the computer's name or IP). */
    juce::String linkFor (int bus) const;
    /** The address of the console app (the iPad's). */
    juce::String consoleLink() const;

    /** The console remote: its PIN, the iPads connected. */
    MixConsole& getConsole() noexcept { return console; }
    juce::String getHostName() const;

    /** The phones connected, by bus. */
    std::array<int, mix::kBuses> connectedByBus();

    /** Bumped when the settings or connections change (the window polls it). */
    int getVersion() const noexcept { return version.load(); }

    /** Tests: the settings file, and an answer without a network. */
    void setSettingsFileForTesting (const juce::File&);
    MixWebServer::Response answer (const juce::String& path) const { return http (path); }
    /** The console's messages are handled now (tests: no message loop). */
    void tickConsole() { console.tick(); }
    /** The mix a page on @p bus is sent (JSON). */
    juce::String mixFor (int bus) const;
    /** Handles a page's message as if it came from a phone on @p bus (applied on the next timer tick, or now with @p now). */
    void receive (int bus, const juce::String& message, bool now = false);
    /** Applies what the phones sent now (tests: no message loop). */
    void flush() { applyPending(); }
    /** The key in bus @p bus's link. */
    juce::String getKey (int bus) const { return keyFor (bus); }

private:
    RKMixProcessor& mixer;
    MixWebServer server;
    MixConsole console;
    juce::File settingsFile;
    bool enabled = false, useIp = false;
    int port = 8080;
    juce::String secret;
    bool portTaken = false;
    std::atomic<int> version { 0 };

    // Changes from the phones, applied on the message thread.
    juce::CriticalSection pendingLock;
    std::vector<std::pair<juce::RangedAudioParameter*, float>> pending;   // real values

    // Each connection's last mix sent (by the pusher thread).
    class Pusher;
    std::unique_ptr<Pusher> pusher;
    std::atomic<int> clientsOpen { 0 }, consolesOpen { 0 };

    juce::MemoryBlock icon;

    void loadSettings();
    void saveSettings() const;
    void restart();
    juce::String keyFor (int bus) const;
    int busForKey (const juce::String&) const;
    MixWebServer::Response http (const juce::String& path) const;
    juce::String meters (int bus) const;
    void timerCallback() override;
    void applyPending();
};
