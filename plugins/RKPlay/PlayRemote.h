#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "remote/RemoteServer.h"

#include <atomic>
#include <map>
#include <memory>
#include <vector>

class RKPlayProcessor;

/** RKPlay's web remote (docs/design/RKPlay Web Remote.md): a page for a
    phone or an iPad, served on the local network (http://<mac>.local:8082).
    Pages: Play (the song, the transport, Level, Duck, End at), Songs (any
    playlist, and a search across all of them), Queue (reorder, remove),
    Lists (switch now or after this song) and Pads. Playback only, as the
    OSC remote: nothing of the show's setup.

    One server per process (a juce::SharedResourcePointer): several RKPlays
    in one host share the port, and the page lists them. PIN-protected as
    RKBoard's remote (a copy of its code): a device that typed the PIN once
    (or scanned the QR code, which carries it) keeps a token; five wrong PINs
    from one address make it wait 30 seconds.

    Protocol (JSON over a WebSocket at /ws/play): after "hello" (pin or
    token) and "open" (a player), the page gets "now" (the song, the
    transport, Level, Duck, End at, Switch at; the position in quarter
    seconds), "lists", "queue" and "pads" when they change, and the songs of
    the playlist it asked for ({"t":"songs",playlist}) when that list changes.
    Commands: play, pause, playpause, next, prev, fade, stop, panic,
    {"t":"level",v}, {"t":"duck",on}, {"t":"pad",n}, {"t":"switch",playlist,
    now}, cancelSwitch, {"t":"queueNext"|"playNow"|"jump",playlist,index,
    file} (the file is checked: a list changed since the page got it never
    plays the wrong song), {"t":"queueMove",from,to}, {"t":"queueRemove",id},
    queueClear, {"t":"endAt",time:"23:30"} or {"t":"endAt",off:true},
    {"t":"search",q}. Each is applied on the message thread, as the same
    click in the window.

    Settings (on, port, PIN, devices) are the computer's, shared by the
    plugin and the app: Application Support/RKFX/RKPlay/remote.json.

    Threads: connections open, close and send on their own threads;
    messages are handled on the message thread (tick(), 20 times a second);
    a push thread sends what waits. */
class PlayRemoteHub : private juce::Timer
{
public:
    PlayRemoteHub();
    ~PlayRemoteHub() override;

    static constexpr int kDefaultPort = 8082;   // RKMix 8080, RKBoard 8081
    static constexpr int kMaxFailures = 5;
    static constexpr int kLockoutSeconds = 30;
    static constexpr int kMaxResults = 200;

    //==============================================================================
    // Players (message thread)

    /** Registers @p player; @p serve: the plugin and the app (starts the server when on). */
    void addPlayer (RKPlayProcessor&, bool serve);
    void removePlayer (RKPlayProcessor&);
    int idOf (const RKPlayProcessor&) const;

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

    /** "On at http://...", "Port 8082 is in use", "Off". */
    juce::String getStatus() const;
    bool isListening() const noexcept { return server.isRunning(); }
    /** The page's address; with @p withPin, the PIN in it (the QR code: no typing). */
    juce::String link (bool withPin) const;

    struct Device
    {
        int id = 0;
        juce::String name, address, player;
        bool signedIn = false;
    };
    std::vector<Device> getDevices();
    /** How many devices are signed in (the window's light). */
    int numSignedIn();
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
    juce::String nowJson (RKPlayProcessor&) const;
    juce::String songsJson (RKPlayProcessor&, int playlist) const;
    juce::String searchJson (RKPlayProcessor&, const juce::String& query) const;
    /** Lower case, without accents ("Café" -> "cafe"): what search compares. */
    static juce::String fold (const juce::String&);
    /** The song's index in @p playlist: @p index if it's still @p file, else where @p file is now; -1: gone. */
    static int findSong (const RKPlayProcessor&, int playlist, int index, const juce::String& file);

private:
    struct Client;
    struct Player
    {
        RKPlayProcessor* processor = nullptr;
        int id = 0;
        int songsVersion = -1;                    // the show version the song lists below were made at
        std::map<int, juce::String> songs;        // by playlist, made when a page wants them
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
    std::vector<std::unique_ptr<Player>> players;
    int nextPlayerId = 1, nextClientId = 1;
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

    Player* findPlayer (int id) const;
    void handle (Client&, const juce::var& message);
    void signIn (Client&, const juce::var& message);
    void command (Client&, Player&, const juce::String& type, const juce::var& message);
    void post (Client&, const juce::String& text);
    juce::String playersJson() const;
    juce::String listsJson (RKPlayProcessor&) const;
    juce::String queueJson (RKPlayProcessor&) const;
    juce::String padsJson (RKPlayProcessor&) const;
    const juce::String& songsOf (Player&, int playlist);

    void timerCallback() override { tick(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayRemoteHub)
};
