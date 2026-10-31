#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_osc/juce_osc.h>

#include "MidiMap.h"

#include <array>
#include <map>
#include <memory>

class RKPlayProcessor;

/** What only the standalone app has (see docs/design/RKPlay.md): the rig's
    settings in Application Support/RKFX/RKPlay (the same for every show),
    remote control by OSC (TouchOSC) and by MIDI (learned controllers,
    program changes), keeping the Mac awake while music plays, an autosave
    every 30 s with recovery after a crash, and a history of what played.
    Message thread only. */
class PlayRig : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    PlayRig (RKPlayProcessor&, juce::File folder);
    ~PlayRig() override;

    static juce::File defaultFolder();

    //==============================================================================
    // OSC: playback control from TouchOSC (or anything on the network), with feedback
    struct Osc
    {
        bool on = false;
        int port = 8000;                 // RKPlay listens here (TouchOSC sends to 8000 by default)
        juce::String phone;              // where the feedback goes: the phone's address
        int phonePort = 9000;            // TouchOSC listens on 9000 by default
    };
    const Osc& getOsc() const noexcept { return osc; }
    void setOsc (const Osc&);            // connects, and saves the rig
    juce::String getOscStatus() const;
    static juce::StringArray localAddresses();

    /** Acts on one message (the receiver calls it; tests too). */
    void handleOsc (const juce::OSCMessage&);

    /** The feedback that changed since the last call (everything: all of it). */
    std::vector<juce::OSCMessage> takeFeedback (bool everything);

    //==============================================================================
    // MIDI: learned controllers (the rig's midi-map.json) and program changes
    enum Target { play, pause, playPause, next, prev, fade, stop, duck, pad1, level = pad1 + 8, numTargets };
    static juce::String targetName (int);
    void learn (int target);             // the next controller moved is this target's
    int getLearning() const noexcept { return learning; }
    void forget (int target);
    juce::String describe (int target) const;   // "CC 20, channel 1", or empty
    void handleMidi (const juce::MidiMessage&);

    //==============================================================================
    // History and recovery
    /** A song stopped being the current one after @p seconds. */
    void songFinished (const juce::String& playlist, const juce::String& title, const juce::String& artist,
                       const juce::File& file, double seconds);
    juce::File historyFile (juce::Time day) const;
    juce::File getFolder() const { return folder; }

    /** Saves the show and what plays (clean: the app is quitting normally). */
    void writeSession (bool clean);

    /** After a crash (the last session wasn't clean): loads it back, the song
        that was playing ready to play. Returns whether it did. */
    bool recover();

    /** The regular work (the processor's timer): keep awake, feedback, autosave, recovery. */
    void update();

    int getVersion() const noexcept { return version; }

private:
    RKPlayProcessor& player;
    juce::File folder;
    Osc osc;
    juce::OSCReceiver receiver;
    juce::OSCSender sender;
    bool listening = false, sending = false;
    std::map<juce::String, juce::var> sent;
    double lastFeedback = 0.0;

    std::unique_ptr<rkfx::MidiMap> midiMap;
    int learning = -1;
    std::array<bool, numTargets> held {};

    bool awake = false;
    double lastAutosave = 0.0;
    int ticks = 0;
    bool recoveryChecked = false;
    int version = 0;

    void oscMessageReceived (const juce::OSCMessage& m) override { handleOsc (m); }
    void connectOsc();
    void loadRig();
    void saveRig() const;
    void apply (int target, int value);
};
