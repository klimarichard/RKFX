#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PitchDetector.h"
#include "host/remote/RemoteServer.h"

#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace rkboard
{

class BoardProcessor;

/** RKBoard's remote: a web app for a phone on a mic stand or an iPad on the
    pedalboard, served on the local network (http://<mac>.local:8081). Pages:
    Scenes (big buttons with their names, the Morph fader), Board (the pedals
    as tiles in signal order: tap the footswitch for bypass, the tile for its
    knobs), Assign (the Assign parameters in use, as faders), Tuner (the app)
    and Tempo (the BPM, a big Tap).

    One server per process (a juce::SharedResourcePointer), so several
    RKBoards in one Gig Performer share the port: each registers under its
    name (the board's remote name, "Board" by default) and the page lists
    them. PIN-protected as RKMix's console remote: a device that typed it
    once (or scanned the QR code, which carries it) keeps a token; five wrong
    PINs from one address make it wait 30 seconds.

    Protocol (JSON over a WebSocket at /ws/board): after "hello" (pin or
    token) and "open" (a board), the page gets the board's state when it
    changes ({"type":"board", ...}), and the values it subscribes to as RKMix's
    console does ({"t":"sub","ids":[...]} -> {"type":"v","p":{id:[normalised,
    text, default, steps, choices]}}). Value IDs: "p<pedal>:<param>",
    "s<split>:<param>", "a<slot>" (Assign 1..32), "morph", "in", "out".
    Commands: {"t":"set",id,v}, {"t":"scene",n}, {"t":"bypass",id,on},
    {"t":"param",owner,id,v}, {"t":"assign",slot,v}, {"t":"morph",v},
    {"t":"morphScenes",from,to}, {"t":"show",id} (whose knobs to send),
    {"t":"tap"}, {"t":"tuner",on}. Each is applied on the message thread,
    into undo and the current scene, as a click in the window would be.

    Settings (on, port, PIN, devices) are the computer's, shared by the
    plugin and the app: Application Support/RKFX/RKBoard/remote.json.

    Threads: connections open, close and send on their own threads; messages
    are handled on the message thread (tick(), 20 times a second); a push
    thread sends what waits. */
class BoardRemoteHub : private juce::Timer
{
public:
    BoardRemoteHub();
    ~BoardRemoteHub() override;

    static constexpr int kDefaultPort = 8081;
    static constexpr int kMaxFailures = 5;
    static constexpr int kLockoutSeconds = 30;

    //==============================================================================
    // Boards (message thread)

    /** Registers @p board; @p serve: the plugin and the app (starts the server when on). */
    void addBoard (BoardProcessor&, bool serve);
    void removeBoard (BoardProcessor&);
    /** A board's number in this process (1 and up), or 0. */
    int idOf (const BoardProcessor&) const;

    //==============================================================================
    // Settings

    bool isEnabled() const noexcept { return enabled; }
    void setEnabled (bool);
    int getPort() const noexcept { return port; }
    void setPort (int);
    bool linksUseIpAddress() const noexcept { return useIp; }
    void setLinksUseIpAddress (bool);
    juce::String getPin() const;
    /** 4 to 8 digits; forgets every device and lets the connected ones go. */
    bool setPin (const juce::String&);

    /** "On at http://...", "Port 8081 is in use", "Off". */
    juce::String getStatus() const;
    bool isListening() const noexcept { return server.isRunning(); }
    /** The page's address; with @p withPin, the PIN in it (the QR code: no typing). */
    juce::String link (bool withPin) const;

    struct Device
    {
        int id = 0;
        juce::String name, address, board;
        bool signedIn = false;
    };
    std::vector<Device> getDevices();
    /** Lets device @p id go and forgets it (it must type the PIN again). */
    void disconnect (int id);
    /** Bumped when the settings or the devices change (the window polls it). */
    int getVersion() const noexcept { return version.load(); }

    //==============================================================================
    // Connections (any thread) and the message thread

    void opened (rkfx::RemoteServer::Connection&);
    void closed (rkfx::RemoteServer::Connection&);
    void received (rkfx::RemoteServer::Connection&, const juce::String& message);
    /** Sends what waits for @p connection (the push thread; tests). */
    void send (rkfx::RemoteServer::Connection&);
    /** Handles what came in, and queues what changed. */
    void tick();

    //==============================================================================
    // Tests
    void setSettingsFileForTesting (const juce::File&);
    rkfx::RemoteServer::Response answer (const juce::String& path) const { return http (path); }
    /** The state message of @p board as a page would get it. */
    juce::String stateJson (BoardProcessor&, int shownPedal) const;

private:
    struct Client;
    struct Board
    {
        BoardProcessor* processor = nullptr;
        int id = 0;
        PitchDetector detector;
        std::vector<float> window;
        float hz = 0.0f;
        juce::uint32 lastTuner = 0;
    };

    rkfx::RemoteServer server;
    juce::File settingsFile;
    bool enabled = false, useIp = false, serving = false, portTaken = false;
    int port = kDefaultPort;
    juce::String pin;

    struct KnownDevice
    {
        juce::String token, name;
        juce::int64 lastSeen = 0;
    };
    std::vector<KnownDevice> known;
    struct Failures
    {
        int count = 0;
        juce::uint32 until = 0;
    };
    std::map<juce::String, Failures> failures;   // by address

    mutable juce::CriticalSection lock;
    std::map<const rkfx::RemoteServer::Connection*, std::unique_ptr<Client>> clients;
    std::vector<std::unique_ptr<Board>> boards;
    int nextBoardId = 1, nextClientId = 1;
    std::atomic<int> version { 0 };

    class Pusher;
    std::unique_ptr<Pusher> pusher;
    juce::MemoryBlock icon;

    void loadSettings();
    void saveSettings() const;
    void restart();
    void settingsChanged();
    juce::String hostName() const;
    rkfx::RemoteServer::Response http (const juce::String& path) const;

    Board* findBoard (int id) const;
    void handle (Client&, const juce::var& message);
    void signIn (Client&, const juce::var& message);
    void command (Client&, Board&, const juce::String& type, const juce::var& message);
    void post (Client&, const juce::String& text);
    juce::String boardsJson() const;
    juce::RangedAudioParameter* resolve (BoardProcessor&, const juce::String& id) const;
    void setValue (BoardProcessor&, const juce::String& id, float normalised);
    void queueValues (Client&, BoardProcessor&);
    void updateTuner (Board&);

    void timerCallback() override { tick(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardRemoteHub)
};

} // namespace rkboard
