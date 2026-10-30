#include "PlayRig.h"

#include "PluginProcessor.h"
#include "plugin/PresetLibrary.h"

namespace
{
    constexpr double kAutosaveSeconds = 30.0;
    constexpr int kRecoveryTicks = 30;          // about a second after start: after the app has restored its own state
    const juce::String kRoot ("/rkplay/");

    juce::String clock (double seconds)
    {
        const auto total = (int) std::floor (juce::jmax (0.0, seconds));
        const auto h = total / 3600;
        return (h > 0 ? juce::String (h) + ":" + juce::String ((total / 60) % 60).paddedLeft ('0', 2) : juce::String (total / 60))
               + ":" + juce::String (total % 60).paddedLeft ('0', 2);
    }

    // An OSC argument as a number (TouchOSC sends floats; a missing one counts as a press).
    float number (const juce::OSCMessage& m, int index = 0, float otherwise = 1.0f)
    {
        if (m.size() <= index)
            return otherwise;
        const auto& a = m[index];
        if (a.isFloat32()) return a.getFloat32();
        if (a.isInt32())   return (float) a.getInt32();
        if (a.isString())  return a.getString().getFloatValue();
        return otherwise;
    }

    juce::String text (const juce::OSCMessage& m, int index = 0)
    {
        if (m.size() <= index)
            return {};
        const auto& a = m[index];
        return a.isString() ? a.getString() : a.isInt32() ? juce::String (a.getInt32()) : a.isFloat32() ? juce::String (a.getFloat32()) : juce::String();
    }

    juce::String csv (const juce::String& s)
    {
        return s.containsAnyOf (",\"\n") ? "\"" + s.replace ("\"", "\"\"") + "\"" : s;
    }
}

PlayRig::PlayRig (RKPlayProcessor& p, juce::File f) : player (p), folder (std::move (f))
{
    receiver.addListener (this);
    loadRig();
}

PlayRig::~PlayRig()
{
    receiver.removeListener (this);
    receiver.disconnect();
    if (awake)
        juce::Desktop::getInstance().setScreenSaverEnabled (true);
}

juce::File PlayRig::defaultFolder()
{
    return rkfx::PresetLibrary::defaultSettingsFile().getParentDirectory().getChildFile ("RKPlay");
}

//==============================================================================
void PlayRig::loadRig()
{
    midiMap = std::make_unique<rkfx::MidiMap> (folder.getChildFile ("midi-map.json"), rkfx::MidiMap::Targets {
        [] (int t) { return t >= 0 && t < numTargets ? targetName (t).toLowerCase().removeCharacters (" /") : juce::String(); },
        [] (const juce::String& key)
        {
            for (int t = 0; t < numTargets; ++t)
                if (targetName (t).toLowerCase().removeCharacters (" /") == key)
                    return t;
            return -1;
        },
        [] (int t) { return t != level; } });

    const auto root = juce::JSON::parse (folder.getChildFile ("settings.json").loadFileAsString());
    const auto& o = root["osc"];
    osc.on = o.getProperty ("on", false);
    osc.port = juce::jlimit (1024, 65535, (int) o.getProperty ("port", 8000));
    osc.phone = o["phone"].toString();
    osc.phonePort = juce::jlimit (1, 65535, (int) o.getProperty ("phonePort", 9000));
    connectOsc();
}

void PlayRig::saveRig() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("on", osc.on);
    o->setProperty ("port", osc.port);
    o->setProperty ("phone", osc.phone);
    o->setProperty ("phonePort", osc.phonePort);
    auto* root = new juce::DynamicObject();
    root->setProperty ("osc", juce::var (o));
    folder.createDirectory();
    rkfx::PresetLibrary::writeAtomically (folder.getChildFile ("settings.json"), juce::JSON::toString (juce::var (root)));
}

//==============================================================================
// OSC

void PlayRig::setOsc (const Osc& o)
{
    osc = o;
    saveRig();
    connectOsc();
    ++version;
}

void PlayRig::connectOsc()
{
    receiver.disconnect();
    sender.disconnect();
    listening = sending = false;
    sent.clear();
    if (! osc.on)
        return;
    listening = receiver.connect (osc.port);
    if (osc.phone.isNotEmpty())
        sending = sender.connect (osc.phone, osc.phonePort);
}

juce::StringArray PlayRig::localAddresses()
{
    juce::StringArray result;
    for (const auto& a : juce::IPAddress::getAllAddresses())
        if (a != juce::IPAddress::local() && ! a.isIPv6)
            result.add (a.toString());
    return result;
}

juce::String PlayRig::getOscStatus() const
{
    if (! osc.on)
        return "Off";
    if (! listening)
        return "Port " + juce::String (osc.port) + " is taken by another app";
    auto s = "Listening on " + localAddresses().joinIntoString (", ") + ", port " + juce::String (osc.port);
    s << (sending ? "; feedback to " + osc.phone + ":" + juce::String (osc.phonePort) : juce::String ("; no feedback (set the phone's address)"));
    return s;
}

void PlayRig::handleOsc (const juce::OSCMessage& m)
{
    auto address = m.getAddressPattern().toString();
    if (! address.startsWith (kRoot))
        return;
    address = address.substring (kRoot.length());
    const auto parts = juce::StringArray::fromTokens (address, "/", {});
    const auto command = parts[0];
    const auto pressed = number (m) >= 0.5f;   // a button's press (its release sends 0)

    if (command == "hello")                       { sent.clear(); return; }   // feedback: everything again
    if (command == "play" && pressed)             { player.play(); return; }
    if (command == "pause" && pressed)            { player.pause(); return; }
    if (command == "playpause" && pressed)        { player.togglePlay(); return; }
    if (command == "next" && pressed)             { player.next(); return; }
    if (command == "prev" && pressed)             { player.previous(); return; }
    if (command == "fade" && pressed)             { player.fadeOut(); return; }
    if (command == "stop" && pressed)             { player.stop(); return; }
    if (command == "panic" && pressed)            { player.panic(); return; }

    if (command == "duck")
    {
        // A toggle button sends 1 and 0; no argument toggles.
        player.setManualDuck (m.isEmpty() ? ! player.isManualDuck() : pressed);
        return;
    }
    if (command == "pad")
    {
        // /rkplay/pad/3 (a button) or /rkplay/pad 3
        const auto pad = (parts.size() > 1 ? parts[1].getIntValue() : (int) number (m, 0, 0.0f)) - 1;
        if ((parts.size() == 1 || pressed) && pad >= 0 && pad < PlayEngine::kPads)
            player.triggerPad (pad);
        return;
    }
    if (command == "playlist")
    {
        // /rkplay/playlist/2 (a button), /rkplay/playlist 2, /rkplay/playlist/after 2
        const auto after = parts[1] == "after";
        const auto n = (parts.size() > 1 && ! after ? parts[1].getIntValue() : (int) number (m, 0, 0.0f)) - 1;
        if ((parts.size() == 1 || after || pressed) && n >= 0 && n < (int) player.getShow().playlists.size())
            player.switchPlaylist (n, ! after);
        return;
    }
    if (command == "level")
    {
        // A fader (0..1, the Level knob's travel), or /rkplay/level/db in dB.
        auto* param = player.findParam ("level");
        if (parts[1] == "db")
            param->setValueNotifyingHost (param->convertTo0to1 ((float) number (m, 0, 0.0f)));
        else
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, number (m, 0, param->getValue())));
        return;
    }
    if (command == "endat")
    {
        // /rkplay/endat "23:30", /rkplay/endat/off
        const auto& endAt = player.getShow().endAt;
        if (parts[1] == "off")
        {
            player.setEndAt (false, endAt.minutes, endAt.atSongEnd);
            return;
        }
        const auto t = juce::StringArray::fromTokens (text (m), ":", {});
        if (t.size() == 2)
            player.setEndAt (true, juce::jlimit (0, 23, t[0].getIntValue()) * 60 + juce::jlimit (0, 59, t[1].getIntValue()), endAt.atSongEnd);
    }
}

std::vector<juce::OSCMessage> PlayRig::takeFeedback (bool everything)
{
    // What a TouchOSC layout shows: buttons light on their own addresses, labels get text.
    std::map<juce::String, juce::var> now;
    const auto state = player.getState();
    const auto& show = player.getShow();
    const auto& item = player.getCurrent();
    const auto stopped = state == RKPlayProcessor::State::stopped || ! item.isValid();
    const auto nextItem = player.peekNext();
    const auto& song = ! stopped ? item.song : nextItem.song;
    const auto position = stopped ? 0.0 : player.getPositionSeconds();
    const auto length = song.lengthSeconds;

    now["play"] = state == RKPlayProcessor::State::playing ? 1.0f : 0.0f;
    now["pause"] = state == RKPlayProcessor::State::paused ? 1.0f : 0.0f;
    now["state"] = juce::String (state == RKPlayProcessor::State::playing ? "Playing" : state == RKPlayProcessor::State::paused ? "Paused" : "Stopped");
    now["title"] = song.title;
    now["artist"] = song.artist;
    now["elapsed"] = clock (position);
    now["remaining"] = stopped ? clock (length) : "-" + clock (juce::jmax (0.0, length - position));
    now["progress"] = length > 0.0 ? (float) juce::jlimit (0.0, 1.0, position / length) : 0.0f;
    now["next"] = ! stopped && nextItem.isValid() ? nextItem.song.displayName() : juce::String();
    now["playlist/name"] = show.current().name;
    for (int i = 0; i < PlayShow::kMaxPlaylists; ++i)
    {
        const auto exists = i < (int) show.playlists.size();
        now["playlist/" + juce::String (i + 1)] = exists && i == show.active ? 1.0f : 0.0f;
        now["playlist/" + juce::String (i + 1) + "/name"] = exists ? show.playlists[(size_t) i].name : juce::String();
    }
    now["duck"] = player.isManualDuck() || player.getDuckDb() < -0.5f ? 1.0f : 0.0f;
    now["duck/db"] = (float) juce::roundToInt (player.getDuckDb());
    for (int pad = 0; pad < PlayEngine::kPads; ++pad)
    {
        now["pad/" + juce::String (pad + 1)] = player.isPadPlaying (pad) ? 1.0f : 0.0f;
        now["pad/" + juce::String (pad + 1) + "/name"] = show.pads[(size_t) pad].name;
    }
    const auto left = player.getEndAtRemaining();
    now["endat"] = left >= 0.0 ? juce::String (show.endAt.minutes / 60).paddedLeft ('0', 2) + ":" + juce::String (show.endAt.minutes % 60).paddedLeft ('0', 2)
                                     + "  in " + clock (left)
                               : juce::String();
    now["level"] = player.findParam ("level")->getValue();

    std::vector<juce::OSCMessage> messages;
    for (const auto& [key, value] : now)
    {
        if (! everything)
            if (const auto it = sent.find (key); it != sent.end() && it->second == value)
                continue;
        sent[key] = value;
        juce::OSCMessage m (juce::OSCAddressPattern (kRoot + key));
        if (value.isString())
            m.addString (value.toString());
        else
            m.addFloat32 ((float) value);
        messages.push_back (std::move (m));
    }
    return messages;
}

//==============================================================================
// MIDI

juce::String PlayRig::targetName (int t)
{
    switch (t)
    {
        case play:      return "Play";
        case pause:     return "Pause";
        case playPause: return "Play/Pause";
        case next:      return "Next";
        case prev:      return "Previous";
        case fade:      return "Fade out";
        case stop:      return "Stop";
        case duck:      return "Duck";
        case level:     return "Level";
        default:        return t >= pad1 && t < pad1 + PlayEngine::kPads ? "Pad " + juce::String (t - pad1 + 1) : juce::String();
    }
}

void PlayRig::learn (int target)
{
    learning = target;
    ++version;
}

void PlayRig::forget (int target)
{
    midiMap->clear (target);
    ++version;
}

juce::String PlayRig::describe (int target) const
{
    if (const auto m = midiMap->find (target))
        return "CC " + juce::String (m->cc) + ", channel " + juce::String (m->channel);
    return {};
}

void PlayRig::handleMidi (const juce::MidiMessage& message)
{
    if (message.isProgramChange())
    {
        // Program changes 0-7: playlists 1-8 (crossfade now; stopped: the one Play starts).
        if (const auto n = message.getProgramChangeNumber(); n < (int) player.getShow().playlists.size())
            player.switchPlaylist (n, true);
        return;
    }
    if (! message.isController())
        return;

    const auto channel = message.getChannel(), cc = message.getControllerNumber(), value = message.getControllerValue();
    if (learning >= 0)
    {
        midiMap->set ({ learning, channel, cc, false });
        learning = -1;
        ++version;
        return;
    }
    if (const auto code = midiMap->lookup (channel, cc); code >= 0)
        apply (code % 1000, value);
}

void PlayRig::apply (int target, int value)
{
    if (target == level)
    {
        auto* param = player.findParam ("level");
        param->setValueNotifyingHost ((float) value / 127.0f);
        return;
    }
    // Switches act as a footswitch goes down (127), not as it comes up.
    const auto pressed = value >= 64;
    const auto edge = pressed && ! held[(size_t) target];
    held[(size_t) target] = pressed;
    if (! edge)
        return;
    switch (target)
    {
        case play:      player.play(); break;
        case pause:     player.pause(); break;
        case playPause: player.togglePlay(); break;
        case next:      player.next(); break;
        case prev:      player.previous(); break;
        case fade:      player.fadeOut(); break;
        case stop:      player.stop(); break;
        case duck:      player.setManualDuck (! player.isManualDuck()); break;
        default:
            if (target >= pad1 && target < pad1 + PlayEngine::kPads)
                player.triggerPad (target - pad1);
            break;
    }
}

//==============================================================================
// History and recovery

juce::File PlayRig::historyFile (juce::Time day) const
{
    return folder.getChildFile ("History").getChildFile (day.formatted ("%Y-%m-%d") + ".csv");
}

void PlayRig::songFinished (const juce::String& playlist, const juce::String& title, const juce::String& artist,
                            const juce::File& file, double seconds)
{
    if (seconds < 1.0)
        return;   // a skip
    const auto now = juce::Time::getCurrentTime();
    const auto log = historyFile (now);
    log.getParentDirectory().createDirectory();
    if (! log.existsAsFile())
        log.appendText ("time,playlist,title,artist,seconds,file\n");
    log.appendText (now.formatted ("%H:%M:%S") + "," + csv (playlist) + "," + csv (title) + "," + csv (artist) + ","
                    + juce::String (juce::roundToInt (seconds)) + "," + csv (file.getFullPathName()) + "\n");
}

void PlayRig::writeSession (bool clean)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("clean", clean);
    root->setProperty ("time", juce::Time::getCurrentTime().toISO8601 (true));
    root->setProperty ("state", player.getPresetVar (true));
    const auto& item = player.getCurrent();
    if (item.isValid() && ! item.queued && player.getState() != RKPlayProcessor::State::stopped)
    {
        auto* playing = new juce::DynamicObject();
        playing->setProperty ("playlist", item.playlist);
        playing->setProperty ("index", item.index);
        playing->setProperty ("title", item.song.displayName());
        root->setProperty ("playing", juce::var (playing));
    }
    folder.createDirectory();
    rkfx::PresetLibrary::writeAtomically (folder.getChildFile ("session.json"), juce::JSON::toString (juce::var (root)));
}

bool PlayRig::recover()
{
    const auto session = juce::JSON::parse (folder.getChildFile ("session.json").loadFileAsString());
    if (! session.isObject() || (bool) session.getProperty ("clean", true))
        return false;

    player.loadPresetVar (session["state"], true);
    const auto& playing = session["playing"];
    if (playing.isObject())
        player.readyToPlay ((int) playing["playlist"], (int) playing["index"]);
    const auto when = juce::Time::fromISO8601 (session["time"].toString());
    player.showNote ("RKPlay stopped unexpectedly (last saved " + when.formatted ("%H:%M") + "): the show is back"
                     + (playing.isObject() ? ", " + playing["title"].toString() + " ready to play" : juce::String()));
    writeSession (false);
    return true;
}

//==============================================================================
void PlayRig::update()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // After a crash, the show comes back (once, after the app has restored its own state).
    if (! recoveryChecked && ++ticks >= kRecoveryTicks)
    {
        recoveryChecked = true;
        recover();
        lastAutosave = now;
    }

    // Awake while music plays: no system or display sleep (a long reception).
    const auto playing = player.getState() != RKPlayProcessor::State::stopped;
    if (playing != awake)
    {
        awake = playing;
        juce::Desktop::getInstance().setScreenSaverEnabled (! awake);
    }

    // Autosave.
    if (recoveryChecked && now - lastAutosave >= kAutosaveSeconds)
    {
        lastAutosave = now;
        writeSession (false);
    }

    // OSC feedback: what changed, ten times a second.
    if (sending && now - lastFeedback >= 0.1)
    {
        lastFeedback = now;
        for (const auto& m : takeFeedback (false))
            sender.send (m);
    }
}
