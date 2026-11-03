#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MixLayout.h"
#include "MixWebServer.h"

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <vector>

class RKMixProcessor;

/** The meters the remote pages get, apart from the window's: the loudest
    peaks (gain) and the most gain reduction (dB, <= 0) since the last look. */
struct MixRemoteMeters
{
    std::array<float, mix::kChannels> channels {}, gate {}, comp {};
    std::array<std::array<float, 2>, mix::kBuses> buses {};
    std::array<float, 2> main {};
    float mono = 0.0f;
    std::array<float, mix::kProcessingChains> chainComp {}, chainLimit {};
};

/** The console remote: the whole console on an iPad (a web app RKMix serves
    at http://<mac>.local:8080/console, the same server as the monitor remote).

    Protected by a PIN, set on the Remote page. A device that typed it once
    gets a token it keeps (the PIN isn't asked again); Disconnect on the
    Remote page, or a new PIN, forgets the device. Five wrong PINs from one
    address make it wait 30 seconds.

    The protocol is general. Every control of the console is a parameter
    (a host parameter or a mix setting, by its ID); a page subscribes to the
    IDs it shows and is sent their values (normalised, with the value as
    text) when they change, and moves them the same way. The structure
    (names, colours, links, bus types, inserts, solo, DCAs, the user pages,
    the scene, undo) is one message sent when it changes; the meters of
    every strip come 20 times a second. Everything else is a command (solo,
    scenes, undo, names...). A change from the iPad is the same as one in the
    window: undo and the scenes take it.

    Threads: connections open, close and send messages on their own threads;
    the messages are handled on the message thread (tick(), from MixRemote's
    timer), whose answers wait in an outbox that the remote's push thread
    sends (send()). */
class MixConsole
{
public:
    /** A console's connection tag (a monitor page's is its bus). */
    static constexpr int kTag = 1000;
    static constexpr int kMaxFailures = 5;
    static constexpr int kLockoutSeconds = 30;

    explicit MixConsole (RKMixProcessor&);
    ~MixConsole();

    //==============================================================================
    // Settings (saved by MixRemote with its own)

    bool isEnabled() const noexcept { return enabled; }
    void setEnabled (bool);
    juce::String getPin() const;
    /** A new PIN (4 to 8 digits) forgets every device and lets the connected ones go. */
    bool setPin (const juce::String&);
    static juce::String randomPin();

    juce::var settingsToVar() const;
    void settingsFromVar (const juce::var&);
    /** The settings changed (MixRemote saves them). */
    std::function<void()> onSettingsChanged;
    /** Something waits to be sent (the push thread wakes up). */
    std::function<void()> onOutbox;

    //==============================================================================
    // Connections (any thread)

    void opened (MixWebServer::Connection&);
    void closed (MixWebServer::Connection&);
    void received (MixWebServer::Connection&, const juce::String& message);

    /** The push thread: sends what waits for @p connection, and @p meters (empty: none). */
    void send (MixWebServer::Connection&, const juce::String& meters);

    //==============================================================================
    // Message thread

    /** Handles what came in, and queues what changed (about 30 times a second). */
    void tick();

    struct Device
    {
        int id = 0;                 // the connection's number in this session
        juce::String name, address;
        bool signedIn = false;
    };
    /** The consoles connected now. */
    std::vector<Device> getDevices();
    /** Lets console @p id go and forgets its device (it must type the PIN again). */
    void disconnect (int id);
    /** Bumped when consoles come, go or sign in. */
    int getVersion() const noexcept { return version.load(); }

    /** The meters message for every console (push thread). */
    static juce::String metersJson (const MixRemoteMeters&);

    /** Tests: the structure message as it would be sent now. */
    juce::String structureJson() const;
    /** Tests: the value message for @p id: [normalised, text, default, steps, (choices)]. */
    juce::var describe (const juce::String& id) const;

private:
    struct Client;
    struct Sub
    {
        juce::String id;
        juce::RangedAudioParameter* param = nullptr;
        float last = -1.0f;
        bool described = false;
    };

    RKMixProcessor& mixer;
    bool enabled = true;
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
        juce::uint32 until = 0;     // Time::getMillisecondCounter
    };
    std::map<juce::String, Failures> failures;   // by address

    juce::CriticalSection lock;
    std::map<const MixWebServer::Connection*, std::unique_ptr<Client>> clients;
    int nextId = 1;
    std::atomic<int> version { 0 };
    int ticks = 0;
    juce::String structure;          // the latest structure message

    juce::RangedAudioParameter* resolve (const juce::String& id) const;
    void handle (Client&, const juce::var& message);
    void signIn (Client&, const juce::var& message);
    void command (Client&, const juce::String& type, const juce::var& message);
    void subscribe (Client&, const juce::var& ids);
    void queueValues (Client&);
    void queueCurves (Client&);
    juce::String curveJson (const juce::String& key) const;
    void post (Client&, const juce::String& text);
    void settingsChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixConsole)
};

