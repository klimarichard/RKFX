#pragma once

#include <juce_osc/juce_osc.h>

#include <functional>
#include <memory>

class RKMixProcessor;

/** Names by OSC: what the show is at (the rackspace and variation, or the
    song and its part), sent by Gig Performer's gig script (or anything that
    sends OSC), shown by the remote pages, the phones' and the iPad's.

        /rkmix/title    "Song 3"           the first line ("" clears it)
        /rkmix/subtitle "Chorus"           the second line
        /rkmix/now      "Song 3" "Chorus"  both at once (the second optional)
        /rkmix/clear                       both gone
        /rkmix/scene    3                  recalls scene 3 (1 to 32)
        /rkmix/marker   "Solo"             a marker in the recording (no name: "Marker n")

    While recording, a new first line (a new rackspace or song) also adds a
    marker named after it (MixRecorderSession::showNameChanged).

    The names are only shown: they aren't saved with the mix, aren't in the
    scenes or the undo history, and are gone when OSC is switched off. The
    settings (on, port, from other computers) are the computer's, saved by
    MixRemote with its own; like the web server, the first RKMix to start
    takes the port. By default only this computer can send (the socket is
    bound to 127.0.0.1): Gig Performer runs on the same Mac.

    Message thread, apart from getNow() (any thread). */
class MixOsc : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    static constexpr int kDefaultPort = 9100;
    static constexpr int kMaxLength = 120;   // characters kept of a name

    explicit MixOsc (RKMixProcessor&);
    ~MixOsc() override;

    //==============================================================================
    // Settings (saved by MixRemote)

    bool isEnabled() const noexcept { return enabled; }
    void setEnabled (bool);
    int getPort() const noexcept { return port; }
    void setPort (int);
    /** Whether other computers may send too (else only this one). */
    bool acceptsFromNetwork() const noexcept { return network; }
    void setAcceptsFromNetwork (bool);

    juce::var settingsToVar() const;
    void settingsFromVar (const juce::var&);
    /** The settings changed (MixRemote saves them). */
    std::function<void()> onSettingsChanged;
    /** The names changed (the remote pages are sent them). */
    std::function<void()> onNowChanged;

    /** Listens, if it's on (the plugin and the app; tests call it to test the socket). */
    void begin();
    bool isListening() const noexcept { return listening; }
    /** "Listening on 127.0.0.1:9100", "Port 9100 is in use", "Off". */
    juce::String getStatus() const;

    //==============================================================================
    // What's showing

    struct Now
    {
        juce::String title, subtitle;
        bool isEmpty() const { return title.isEmpty() && subtitle.isEmpty(); }
        bool operator== (const Now& o) const { return title == o.title && subtitle == o.subtitle; }
        bool operator!= (const Now& o) const { return ! operator== (o); }
    };
    /** Any thread. */
    Now getNow() const;
    void setNow (const Now&);

    /** The "now" object of the remote pages' messages: {"t":"...","s":"..."}. Any thread. */
    juce::String nowJson() const;

    /** Acts on one message (the receiver calls it; tests too). */
    void handle (const juce::OSCMessage&);

private:
    RKMixProcessor& mixer;
    juce::OSCReceiver receiver;
    std::unique_ptr<juce::DatagramSocket> socket;
    bool enabled = false, network = false, started = false, listening = false, portTaken = false;
    int port = kDefaultPort;

    mutable juce::CriticalSection nowLock;
    Now now;

    void connect();
    void oscMessageReceived (const juce::OSCMessage&) override;
    void oscBundleReceived (const juce::OSCBundle&) override;
    void settingsChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixOsc)
};
