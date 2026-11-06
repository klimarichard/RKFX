#include "MixConsole.h"
#include "MixOsc.h"

#include "PluginProcessor.h"
#include "host/PedalRegistry.h"

#include <cmath>

using namespace mix;

namespace
{
    juce::String jsonString (const juce::String& s) { return juce::JSON::toString (juce::var (s)); }
    juce::String flag (bool b) { return b ? "true" : "false"; }

    juce::String number (float v, int decimals)
    {
        if (! std::isfinite (v))
            return "0";
        auto s = juce::String (v, decimals);
        if (s.containsChar ('.'))
            s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s == "-0" ? juce::String ("0") : s;
    }

    /** "ch3" -> 2, "bus2" -> 1 (with the prefix), or -1. */
    int indexAfter (const juce::String& key, const char* prefix, int count)
    {
        if (! key.startsWith (prefix))
            return -1;
        const auto rest = key.substring ((int) std::strlen (prefix));
        if (rest.isEmpty() || ! rest.containsOnly ("0123456789"))
            return -1;
        const auto n = rest.getIntValue() - 1;
        return juce::isPositiveAndBelow (n, count) ? n : -1;
    }

    juce::String hexOf (juce::Colour c) { return "#" + c.toDisplayString (false).toLowerCase(); }
}

//==============================================================================
struct MixConsole::Client
{
    MixWebServer::Connection* connection = nullptr;
    int id = 0;
    juce::String address, name, token;
    bool signedIn = false;
    juce::StringArray inbox, outbox;
    std::vector<Sub> subs;
    juce::StringArray curves;
    std::map<juce::String, juce::String> curvesSent;
    juce::String structureSent;
    int rackVersion = -1;
    juce::var clipboard;              // a copied insert
    bool monitorEq = false;           // shows Monitor EQ: gets its state
    int monitorEqSent = -1;           // the session's version last sent
};

//==============================================================================
MixConsole::MixConsole (RKMixProcessor& p) : mixer (p) {}

MixConsole::~MixConsole() = default;

juce::String MixConsole::randomPin()
{
    auto& random = juce::Random::getSystemRandom();
    juce::String s;
    for (int i = 0; i < 4; ++i)
        s << random.nextInt (10);
    return s;
}

void MixConsole::setEnabled (bool on)
{
    if (on == enabled)
        return;
    enabled = on;
    if (! on)
    {
        const juce::ScopedLock sl (lock);
        for (auto& [c, client] : clients)
            client->connection->close();
    }
    settingsChanged();
}

juce::String MixConsole::getPin() const
{
    const juce::ScopedLock sl (lock);
    return pin;
}

bool MixConsole::setPin (const juce::String& newPin)
{
    const auto digits = newPin.trim();
    if (digits.length() < 4 || digits.length() > 8 || ! digits.containsOnly ("0123456789"))
        return false;
    {
        const juce::ScopedLock sl (lock);
        if (digits == pin)
            return true;
        pin = digits;
        known.clear();   // every device must type the new PIN
        failures.clear();
        for (auto& [c, client] : clients)
            client->connection->close();
    }
    settingsChanged();
    return true;
}

juce::var MixConsole::settingsToVar() const
{
    const juce::ScopedLock sl (lock);
    auto* root = new juce::DynamicObject();
    root->setProperty ("enabled", enabled);
    root->setProperty ("pin", pin);
    juce::Array<juce::var> devices;
    for (const auto& d : known)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("token", d.token);
        o->setProperty ("name", d.name);
        o->setProperty ("lastSeen", d.lastSeen);
        devices.add (juce::var (o));
    }
    root->setProperty ("devices", devices);
    return juce::var (root);
}

void MixConsole::settingsFromVar (const juce::var& v)
{
    const juce::ScopedLock sl (lock);
    enabled = (bool) v.getProperty ("enabled", true);
    pin = v.getProperty ("pin", {}).toString();
    known.clear();
    if (const auto* devices = v.getProperty ("devices", {}).getArray())
        for (const auto& d : *devices)
            if (const auto token = d.getProperty ("token", {}).toString(); token.length() >= 16)
                known.push_back ({ token, d.getProperty ("name", {}).toString(), (juce::int64) d.getProperty ("lastSeen", 0) });
    if (pin.length() < 4 || ! pin.containsOnly ("0123456789"))
    {
        pin = randomPin();
        known.clear();
    }
}

void MixConsole::settingsChanged()
{
    ++version;
    if (onSettingsChanged != nullptr)
        onSettingsChanged();
}

//==============================================================================
void MixConsole::opened (MixWebServer::Connection& c)
{
    const juce::ScopedLock sl (lock);
    auto client = std::make_unique<Client>();
    client->connection = &c;
    client->id = nextId++;
    client->address = c.address;
    clients[&c] = std::move (client);
    ++version;
}

void MixConsole::closed (MixWebServer::Connection& c)
{
    const juce::ScopedLock sl (lock);
    clients.erase (&c);
    ++version;
}

void MixConsole::received (MixWebServer::Connection& c, const juce::String& message)
{
    const juce::ScopedLock sl (lock);
    if (auto it = clients.find (&c); it != clients.end() && it->second->inbox.size() < 2000)
        it->second->inbox.add (message);
}

void MixConsole::send (MixWebServer::Connection& c, const juce::String& meters)
{
    juce::StringArray out;
    bool signedIn = false;
    {
        const juce::ScopedLock sl (lock);
        if (auto it = clients.find (&c); it != clients.end())
        {
            out.swapWith (it->second->outbox);
            signedIn = it->second->signedIn;
        }
    }
    for (const auto& text : out)
        c.send (text);
    if (signedIn && meters.isNotEmpty())
        c.send (meters);
}

void MixConsole::post (Client& client, const juce::String& text)
{
    if (client.outbox.size() > 500)   // a device that stopped reading: let it go
    {
        client.connection->close();
        return;
    }
    client.outbox.add (text);
}

std::vector<MixConsole::Device> MixConsole::getDevices()
{
    const juce::ScopedLock sl (lock);
    std::vector<Device> devices;
    for (auto& [c, client] : clients)
        devices.push_back ({ client->id, client->name, client->address, client->signedIn });
    std::sort (devices.begin(), devices.end(), [] (const Device& a, const Device& b) { return a.id < b.id; });
    return devices;
}

void MixConsole::disconnect (int id)
{
    {
        const juce::ScopedLock sl (lock);
        for (auto& [c, client] : clients)
            if (client->id == id)
            {
                const auto token = client->token;
                known.erase (std::remove_if (known.begin(), known.end(), [&] (const KnownDevice& d) { return d.token == token; }),
                             known.end());
                client->signedIn = false;
                client->connection->close();
            }
    }
    settingsChanged();
}

//==============================================================================
void MixConsole::tick()
{
    bool posted = false;
    {
        const juce::ScopedLock sl (lock);
        const auto refreshStructure = ticks++ % 3 == 0 || structure.isEmpty();
        if (refreshStructure && ! clients.empty())
            structure = structureJson();
        checkMonitorEqOwner();
        juce::String monitorEqState;      // made once per tick, when some console wants it

        for (auto& [c, clientPtr] : clients)
        {
            auto& client = *clientPtr;
            const auto before = client.outbox.size();
            juce::StringArray messages;
            messages.swapWith (client.inbox);
            for (const auto& text : messages)
            {
                juce::var message;
                if (juce::JSON::parse (text, message).wasOk() && message.isObject())
                    handle (client, message);
            }
            if (client.signedIn)
            {
                // Inserts came or went: their parameters again.
                if (const auto v = mixer.getRack().getVersion(); v != client.rackVersion)
                {
                    client.rackVersion = v;
                    for (auto& sub : client.subs)
                        if (sub.id.startsWith ("ins:"))
                            if (auto* p = resolve (sub.id); p != sub.param)
                            {
                                sub.param = p;
                                sub.described = false;
                            }
                }
                if (client.structureSent != structure)
                {
                    client.structureSent = structure;
                    post (client, structure);
                }
                queueValues (client);
                queueCurves (client);
                if (const auto v = mixer.getMonitorEq().getVersion(); client.monitorEq && v != client.monitorEqSent)
                {
                    if (monitorEqState.isEmpty())
                        monitorEqState = monitorEqJson();
                    client.monitorEqSent = v;
                    post (client, monitorEqState);
                }
            }
            posted = posted || client.outbox.size() != before;
        }
    }
    if (posted && onOutbox != nullptr)
        onOutbox();
}

void MixConsole::handle (Client& client, const juce::var& message)
{
    const auto type = message["t"].toString();
    if (type == "hello")
        return signIn (client, message);
    if (! client.signedIn)
        return;
    command (client, type, message);
}

void MixConsole::signIn (Client& client, const juce::var& message)
{
    client.name = message["name"].toString().substring (0, 40);
    if (! enabled)
    {
        post (client, R"({"type":"auth","ok":false,"off":true})");
        return;
    }

    const auto token = message["token"].toString();
    const auto now = juce::Time::currentTimeMillis();
    if (token.length() >= 16)
        for (auto& d : known)
            if (d.token == token)
            {
                d.lastSeen = now;
                if (client.name.isNotEmpty())
                    d.name = client.name;
                client.token = token;
                client.signedIn = true;
                ++version;
                post (client, R"({"type":"auth","ok":true})");
                post (client, typesJson());
                return;
            }

    const auto typed = message["pin"].toString();
    if (typed.isEmpty())
    {
        post (client, R"({"type":"auth","ok":false})");
        return;
    }

    auto& f = failures[client.address];
    const auto ms = juce::Time::getMillisecondCounter();
    if (f.until != 0 && ms < f.until)
    {
        post (client, R"({"type":"auth","ok":false,"wait":)" + juce::String ((int) ((f.until - ms) / 1000) + 1) + "}");
        return;
    }
    if (typed != pin)
    {
        if (++f.count >= kMaxFailures)
        {
            f.count = 0;
            f.until = ms + (juce::uint32) kLockoutSeconds * 1000;
            post (client, R"({"type":"auth","ok":false,"wrong":true,"wait":)" + juce::String (kLockoutSeconds) + "}");
        }
        else
            post (client, R"({"type":"auth","ok":false,"wrong":true})");
        return;
    }

    failures.erase (client.address);
    client.token = juce::Uuid().toString() + juce::Uuid().toString().substring (0, 8);
    known.push_back ({ client.token, client.name, now });
    while (known.size() > 16)   // the longest unseen goes
        known.erase (std::min_element (known.begin(), known.end(),
                                       [] (const KnownDevice& a, const KnownDevice& b) { return a.lastSeen < b.lastSeen; }));
    client.signedIn = true;
    post (client, R"({"type":"auth","ok":true,"token":")" + client.token + "\"}");
    post (client, typesJson());
    settingsChanged();
}

//==============================================================================
juce::RangedAudioParameter* MixConsole::resolve (const juce::String& id) const
{
    if (id == "bypass")
        return nullptr;   // the plugin's own bypass is the host's
    if (id.startsWith ("ins:"))
    {
        const auto flat = id.fromFirstOccurrenceOf ("ins:", false, false).upToFirstOccurrenceOf (":", false, false);
        const auto pid = id.fromFirstOccurrenceOf ("ins:", false, false).fromFirstOccurrenceOf (":", false, false);
        if (! flat.containsOnly ("0123456789") || flat.isEmpty() || pid == "bypass")
            return nullptr;   // an insert's bypass is its On, a command
        const auto n = flat.getIntValue();
        if (! juce::isPositiveAndBelow (n, InsertRack::kTotalSlots))
            return nullptr;
        auto* plugin = mixer.getRack().get (InsertRack::slotAt (n));
        return plugin != nullptr ? plugin->findParam (pid) : nullptr;
    }
    if (auto* p = mixer.findParam (id))
        return p;
    return mixer.getSettings().findParam (id);
}

void MixConsole::subscribe (Client& client, const juce::var& ids)
{
    std::vector<Sub> next;
    if (const auto* list = ids.getArray())
        for (const auto& v : *list)
        {
            const auto id = v.toString();
            if (id.isEmpty() || next.size() >= 2000)
                continue;
            auto it = std::find_if (client.subs.begin(), client.subs.end(), [&] (const Sub& s) { return s.id == id; });
            if (it != client.subs.end())
                next.push_back (*it);
            else
                next.push_back ({ id, resolve (id), -1.0f, false });
        }
    client.subs = std::move (next);
}

juce::var MixConsole::describe (const juce::String& id) const
{
    auto* p = resolve (id);
    if (p == nullptr)
        return {};
    juce::Array<juce::var> d { p->getValue(), p->getCurrentValueAsText(), p->getDefaultValue() };
    const auto steps = p->getNumSteps();
    d.add (steps > 1 && steps < 0x7fffffff ? steps : 0);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
    {
        juce::Array<juce::var> names;
        for (const auto& c : choice->choices)
            names.add (c);
        d.add (names);
    }
    return d;
}

void MixConsole::queueValues (Client& client)
{
    juce::String json;
    for (auto& sub : client.subs)
    {
        if (sub.param == nullptr)
            continue;
        const auto v = sub.param->getValue();
        if (sub.described && std::abs (v - sub.last) < 1.0e-6f)
            continue;
        json << (json.isEmpty() ? "" : ",") << jsonString (sub.id) << ":[" << number (v, 6) << "," << jsonString (sub.param->getCurrentValueAsText());
        if (! sub.described)
        {
            const auto steps = sub.param->getNumSteps();
            json << "," << number (sub.param->getDefaultValue(), 6) << "," << (steps > 1 && steps < 0x7fffffff ? steps : 0);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (sub.param))
            {
                json << ",[";
                for (int i = 0; i < choice->choices.size(); ++i)
                    json << (i > 0 ? "," : "") << jsonString (choice->choices[i]);
                json << "]";
            }
            sub.described = true;
        }
        json << "]";
        sub.last = v;
    }
    if (json.isNotEmpty())
        post (client, R"({"type":"v","p":{)" + json + "}}");
}

//==============================================================================
juce::String MixConsole::curveJson (const juce::String& key) const
{
    // "eq:ch3", "comp:ch3": a channel's EQ (with its HPF) and compressor;
    // "eq:bus2", "eq:main", "eq:mono": a chain's EQ; "seq:ch3:bus5": a send's EQ.
    const auto kind = key.upToFirstOccurrenceOf (":", false, false);
    const auto strip = key.fromFirstOccurrenceOf (":", false, false);
    const auto rate = mixer.getSampleRate() > 0.0 ? mixer.getSampleRate() : 48000.0;
    constexpr int points = 128;

    juce::String json;
    const auto add = [&json] (double v) { json << (json.isEmpty() ? "" : ",") << number ((float) v, 2); };
    const auto ch = indexAfter (strip, "ch", kChannels);
    const auto chain = strip == "main" ? kMainChain : strip == "mono" ? kMonoChain : indexAfter (strip, "bus", kBuses);

    if (kind == "eq" && (ch >= 0 || chain >= 0))
    {
        const auto channelStrip = ch >= 0 ? mixer.channelSettings (ch).strip : StripDsp::Settings {};
        const auto chainEq = chain >= 0 ? mixer.chainSettings (chain).eq : BusDsp::Eq {};
        for (int i = 0; i < points; ++i)
        {
            const auto hz = 20.0 * std::pow (1000.0, (double) i / (points - 1));
            add (ch >= 0 ? StripDsp::responseDb (channelStrip, hz, rate) : BusDsp::responseDb (chainEq, hz, rate));
        }
    }
    else if (kind == "seq")
    {
        // "seq:ch3:bus5": channel 3's send EQ to bus 5 (flat while off).
        const auto sendCh = indexAfter (strip.upToFirstOccurrenceOf (":", false, false), "ch", kChannels);
        const auto sendBus = indexAfter (strip.fromFirstOccurrenceOf (":", false, false), "bus", kBuses);
        if (sendCh < 0 || sendBus < 0)
            return {};
        const auto eq = mixer.getSendEq (sendCh, sendBus);
        for (int i = 0; i < points; ++i)
            add (SendEqDsp::responseDb (eq, 20.0 * std::pow (1000.0, (double) i / (points - 1)), rate));
    }
    else if (kind == "comp" && ch >= 0)
    {
        const auto comp = mixer.channelSettings (ch).strip.comp;
        for (int i = 0; i <= 120; ++i)
            add (StripDsp::compTransferDb (comp, -60.0f + 0.5f * (float) i));
    }
    else
        return {};
    return "[" + json + "]";
}

void MixConsole::queueCurves (Client& client)
{
    for (const auto& key : client.curves)
    {
        const auto curve = curveJson (key);
        if (curve.isEmpty() || client.curvesSent[key] == curve)
            continue;
        client.curvesSent[key] = curve;
        post (client, R"({"type":"curve","k":)" + jsonString (key) + R"(,"d":)" + curve + "}");
    }
}

//==============================================================================
juce::String MixConsole::structureJson() const
{
    auto& rack = mixer.getRack();
    auto& s = mixer.getSettings();
    juce::String json;

    const auto inserts = [&] (InsertRack::Strip strip, int index)
    {
        juce::String out = "[";
        for (int slot = 0; slot < InsertRack::numSlots (strip); ++slot)
        {
            const InsertRack::Slot where { strip, index, slot };
            out << (slot > 0 ? "," : "");
            if (const auto* type = rack.typeOf (where))
                out << R"({"n":)" << jsonString (type->shortName) << R"(,"p":)" << jsonString (type->name)
                    << R"(,"f":")" << hexOf (rkfx::Palette::familyColour (type->family)) << R"(","on":)" << flag (rack.isOn (where))
                    << R"(,"midi":)" << flag (type->sendsMidi) << R"(,"x":)" << flag (type->sendsMidi && rack.isInConflict (where)) << "}";
            else
                out << "null";
        }
        return out + "]";
    };

    json << R"({"type":"s","ch":[)";
    for (int ch = 0; ch < kChannels; ++ch)
    {
        const auto linked = mixer.isLinked (ch / 2);
        const auto stereo = ch % 2 == 0 && linked;
        json << (ch > 0 ? "," : "") << R"({"n":)" << jsonString (mixer.getChannelName (ch))
             << R"(,"c":)" << mixer.getChannelColour (ch)
             << R"(,"st":)" << flag (stereo) << R"(,"hid":)" << flag (ch % 2 == 1 && linked)
             << R"(,"gm":)" << flag (mixer.isChannelGroupMuted (ch)) << R"(,"so":)" << flag (mixer.isChannelSoloed (ch))
             << R"(,"sec":[)" << flag (s.flag (channelId (ch, "hpf_on"))) << "," << flag (s.flag (channelId (ch, "gate_on")))
             << "," << flag (s.flag (channelId (ch, "eq_on"))) << "," << flag (s.flag (channelId (ch, "comp_on"))) << "]"
             << R"(,"ins":)" << inserts (InsertRack::Strip::channel, ch) << "}";
    }
    json << R"(],"bus":[)";
    for (int bus = 0; bus < kBuses; ++bus)
    {
        const auto own = mixer.getBusType (bus) == BusType::aux && mixer.isBusStereo (bus)
                      && s.choice (busId (bus, "panmode")) == (int) PanMode::own;
        json << (bus > 0 ? "," : "") << R"({"n":)" << jsonString (mixer.getBusName (bus))
             << R"(,"c":)" << mixer.getBusColour (bus) << R"(,"t":)" << (int) mixer.getBusType (bus)
             << R"(,"st":)" << flag (mixer.isBusStereo (bus)) << R"(,"own":)" << flag (own)
             << R"(,"gm":)" << flag (mixer.isBusGroupMuted (bus)) << R"(,"so":)" << flag (mixer.isBusSoloed (bus))
             << R"(,"ins":)" << inserts (InsertRack::Strip::bus, bus) << "}";
    }
    json << R"(],"main":{"out":)" << jsonString (targetName (s.choice ("main_out")))
         << R"(,"ins":)" << inserts (InsertRack::Strip::main, 0)
         << R"(},"mono":{"out":)" << jsonString (targetName (s.choice ("mono_out")))
         << R"(,"ins":)" << inserts (InsertRack::Strip::mono, 0) << "}";

    const auto members = [] (const juce::Array<int>& list)
    {
        juce::String out = "[";
        for (int i = 0; i < list.size(); ++i)
            out << (i > 0 ? "," : "") << list[i];
        return out + "]";
    };
    json << R"(,"dca":[)";
    for (int d = 0; d < kDcas; ++d)
        json << (d > 0 ? "," : "") << R"({"n":)" << jsonString (mixer.getDcaName (d)) << R"(,"c":)" << mixer.getDcaColour (d)
             << R"(,"m":)" << members (mixer.getDcaMembers (d)) << "}";
    json << R"(],"mg":[)";
    for (int g = 0; g < kMuteGroups; ++g)
        json << (g > 0 ? "," : "") << R"({"n":)" << jsonString (mixer.getMuteGroupName (g))
             << R"(,"m":)" << members (mixer.getMuteGroupMembers (g)) << "}";
    json << R"(],"up":[)" << members (mixer.getUserPage (0)) << "," << members (mixer.getUserPage (1)) << "]";

    auto& undo = mixer.getUndo();
    json << R"(,"scene":)" << mixer.getScenes().getCurrent()
         << R"(,"now":)" << mixer.getRemote().getOsc().nowJson()
         << R"(,"undo":)" << jsonString (undo.canUndo() ? undo.undoDescription() : juce::String())
         << R"(,"redo":)" << jsonString (undo.canRedo() ? undo.redoDescription() : juce::String())
         << R"(,"anySolo":)" << flag (mixer.isAnythingSoloed())
         << R"(,"talkSrc":)" << flag (s.choice ("talk_src") != 0)
         << R"(,"rate":)" << (int) (mixer.getSampleRate() > 0.0 ? mixer.getSampleRate() : 48000.0) << "}";
    return json;
}

//==============================================================================
void MixConsole::command (Client& client, const juce::String& type, const juce::var& m)
{
    const auto strip = m["s"].toString();
    const auto ch = indexAfter (strip, "ch", kChannels);
    const auto bus = indexAfter (strip, "bus", kBuses);

    if (type == "sub")
        subscribe (client, m["ids"]);
    else if (type == "curves")
    {
        client.curves.clear();
        if (const auto* list = m["k"].getArray())
            for (const auto& k : *list)
                if (client.curves.size() < 8)
                    client.curves.addIfNotAlreadyThere (k.toString());
        client.curvesSent.clear();
    }
    else if (type == "set")
    {
        auto* p = resolve (m["id"].toString());
        const auto v = (float) (double) m["v"];
        if (p != nullptr && std::isfinite (v))
        {
            const auto n = juce::jlimit (0.0f, 1.0f, v);
            if (! juce::exactlyEqual (p->getValue(), n))
                p->setValueNotifyingHost (n);
        }
    }
    else if (type == "solo")
    {
        if (ch >= 0)
            mixer.setChannelSolo (ch, (bool) m["on"]);
        else if (bus >= 0)
            mixer.setBusSolo (bus, (bool) m["on"]);
    }
    else if (type == "clearSolo")
        mixer.clearSolo();
    else if (type == "scene")
    {
        const auto n = (int) m["n"];
        if (juce::isPositiveAndBelow (n, MixScenes::kNumScenes))
            mixer.recallScene (n);
    }
    else if (type == "undo" && mixer.getUndo().canUndo())
        mixer.getUndo().undo();
    else if (type == "redo" && mixer.getUndo().canRedo())
        mixer.getUndo().redo();
    else if (type == "name")
    {
        const auto text = m["text"].toString().substring (0, 32);
        if (ch >= 0)
            mixer.setChannelName (ch, text);
        else if (bus >= 0)
            mixer.setBusName (bus, text);
        else if (const auto d = indexAfter (strip, "dca", kDcas); d >= 0)
            mixer.setDcaName (d, text);
        else if (const auto g = indexAfter (strip, "mg", kMuteGroups); g >= 0)
            mixer.setMuteGroupName (g, text);
    }
    else if (type == "colour")
    {
        const auto c = juce::jlimit (0, RKMixProcessor::kNumColours - 1, (int) m["c"]);
        if (ch >= 0)
            mixer.setChannelColour (ch, c);
        else if (bus >= 0)
            mixer.setBusColour (bus, c);
        else if (const auto d = indexAfter (strip, "dca", kDcas); d >= 0)
            mixer.setDcaColour (d, c);
    }
    else if (type.startsWith ("ins"))
        insertCommand (client, type, m);
    else if (type == "userPage")
    {
        // A user page's strips, in order: channels (a pair's left) and kUserPageBus + bus.
        const auto page = (int) m["p"];
        juce::Array<int> strips;
        if (const auto* list = m["list"].getArray())
            for (const auto& v : *list)
            {
                const auto id = (int) v;
                const auto valid = juce::isPositiveAndBelow (id, kChannels)
                                || juce::isPositiveAndBelow (id - RKMixProcessor::kUserPageBus, kBuses);
                if (valid && ! strips.contains (id) && strips.size() < RKMixProcessor::kUserPageStrips)
                    strips.add (id);
            }
        if (juce::isPositiveAndBelow (page, RKMixProcessor::kUserPages))
            mixer.setUserPage (page, strips);
    }
    else if (type == "copy" || type == "paste")
    {
        // A section of a channel or a chain (bus, main, mono): "sec" is input, delay,
        // hpf, gate, eq, comp, limit or whole.
        const auto chain = strip == "main" ? kMainChain : strip == "mono" ? kMonoChain : bus;
        if (type == "copy")
        {
            static const juce::StringArray names { "input", "delay", "hpf", "gate", "eq", "comp", "limit", "whole" };
            const auto sec = names.indexOf (m["sec"].toString());
            if (sec < 0)
                return;
            const auto section = (RKMixProcessor::Section) sec;
            if (ch >= 0)
                client.clipboard = mixer.copyChannel (ch, section);
            else if (chain >= 0)
                client.clipboard = mixer.copyChain (chain, section);
            else
                return;
        }
        else
        {
            if (ch >= 0 && RKMixProcessor::isChannelCopy (client.clipboard))
                mixer.pasteChannel (ch, client.clipboard);
            else if (chain >= 0 && RKMixProcessor::isChainCopy (client.clipboard))
                mixer.pasteChain (chain, client.clipboard);
            return;
        }
        const auto kind = RKMixProcessor::isChannelCopy (client.clipboard) ? "channel" : RKMixProcessor::isChainCopy (client.clipboard) ? "chain" : "";
        post (client, R"({"type":"clip","kind":")" + juce::String (kind) + R"(","text":)"
                          + jsonString (RKMixProcessor::describeCopy (client.clipboard)) + "}");
    }
    else if (type == "ping")
        post (client, R"({"type":"pong"})");
    else if (type == "meq")
        monitorEqCommand (client, m);
}

//==============================================================================
// Monitor EQ (docs/design/RKMix Monitor EQ.md): any Aux bus, from the iPad.

void MixConsole::monitorEqCommand (Client& client, const juce::var& m)
{
    auto& session = mixer.getMonitorEq();
    const auto a = m["a"].toString();
    if (a == "sub")
    {
        client.monitorEq = (bool) m["on"];
        client.monitorEqSent = -1;
        return;
    }
    if (a == "start")
    {
        const auto bus = (int) m["bus"];
        if (juce::isPositiveAndBelow (bus, kBuses) && session.start (bus))
        {
            session.setOwner (client.token);
            ownerGoneSince = 0;
        }
    }
    else if (a == "next")   session.next();
    else if (a == "finish") session.finish();
    else if (a == "stop")   session.stop();
    else if (a == "apply")  session.apply();
    else if (a == "undo")   session.undo();
    else if (a == "clear")  session.clear();
    else if (a == "opt" && m["o"].isObject())
    {
        // Only what is sent changes; the session clamps the rest.
        auto o = session.getOptions();
        const auto& v = m["o"];
        const auto has = [&v] (const char* key) { return v.hasProperty (key); };
        const auto in = [] (const juce::var& x, int count) { return juce::jlimit (0, count - 1, (int) x); };
        if (has ("type"))   o.type = (monitoreq::Type) in (v["type"], 2);
        if (has ("input"))  o.input = in (v["input"], kChannels);
        if (has ("mic"))    { o.mic = (monitoreq::Mic) in (v["mic"], 4); o.useCalibration = false; }
        if (has ("useCal")) o.useCalibration = (bool) v["useCal"];
        if (has ("target")) (o.type == monitoreq::Type::speaker ? o.speakerTarget : o.inEarTarget) = (monitoreq::Target) in (v["target"], 5);
        if (has ("bass"))   o.bassDb = (float) (double) v["bass"];
        if (has ("level"))  (o.type == monitoreq::Type::speaker ? o.speakerLevelDb : o.inEarLevelDb) = (float) (double) v["level"];
        if (has ("sides"))  o.sides = (MixMonitorEqSession::Sides) in (v["sides"], 4);
        if (has ("positions")) (o.type == monitoreq::Type::speaker ? o.speakerPositions : o.inEarPositions) = (int) v["positions"];
        if (has ("strength")) o.strength = (float) (double) v["strength"];
        if (has ("useFile")) o.useFile = (bool) v["useFile"];
        for (auto* f : { &o.bassDb, &o.speakerLevelDb, &o.inEarLevelDb, &o.strength })
            if (! std::isfinite (*f))
                return;
        session.setOptions (o);
    }
}

void MixConsole::checkMonitorEqOwner()
{
    // A run started from an iPad stops when that iPad has been gone for 2 s
    // (a reconnect with the same token within that keeps it going).
    auto& session = mixer.getMonitorEq();
    const auto& owner = session.getOwner();
    if (! session.isRunning() || owner.isEmpty())
    {
        ownerGoneSince = 0;
        return;
    }
    bool present = false;
    for (auto& [c, client] : clients)
        present = present || (client->signedIn && client->token == owner);
    const auto now = juce::jmax (1u, juce::Time::getMillisecondCounter());
    if (present)
        ownerGoneSince = 0;
    else if (ownerGoneSince == 0)
        ownerGoneSince = now;
    else if (now - ownerGoneSince > kOwnerGoneMs)
    {
        ownerGoneSince = 0;
        session.stop ("The iPad that started the measurement disconnected; it stopped.");
    }
}

juce::String MixConsole::monitorEqJson() const
{
    const auto& session = mixer.getMonitorEq();
    const auto& o = session.getOptions();
    static const char* steps[] = { "idle", "floor", "settling", "measuring", "waiting", "done" };
    const auto bands = [] (const monitoreq::Bands& b)
    {
        juce::String list = "[";
        for (size_t i = 0; i < b.size(); ++i)
            list << (i > 0 ? "," : "") << number (b[i], 2);
        return list + "]";
    };
    const auto flags = [] (const monitoreq::Flags& f)
    {
        juce::String bits;
        for (auto v : f)
            bits << (v ? "1" : "0");
        return "\"" + bits + "\"";
    };

    juce::String json;
    json << R"({"type":"meq","v":)" << session.getVersion() << R"(,"bus":)" << session.getBus()
         << R"(,"step":")" << steps[(int) session.getStep()] << "\""
         << R"(,"pos":)" << session.positionsMeasured() << R"(,"of":)" << session.currentPositions()
         << R"(,"prog":)" << number (session.getProgress(), 2)
         << R"(,"msg":)" << jsonString (session.getMessage()) << R"(,"desc":)" << jsonString (session.describeStep())
         << R"(,"ear":)" << number (session.getEarDifferenceDb(), 1)
         << R"(,"res":)" << flag (session.hasResult()) << R"(,"apply":)" << flag (session.canApply())
         << R"(,"why":)" << jsonString (session.applyProblem()) << R"(,"undo":)" << flag (session.canUndo())
         << R"(,"remote":)" << flag (session.getOwner().isNotEmpty());

    json << R"(,"o":{"type":)" << (int) o.type << R"(,"input":)" << o.input << R"(,"mic":)" << (int) o.mic
         << R"(,"useCal":)" << flag (o.useCalibration) << R"(,"hasCal":)" << flag (o.hasCalibration)
         << R"(,"calName":)" << jsonString (o.calibrationName) << R"(,"target":)" << (int) session.currentTarget()
         << R"(,"bass":)" << number (o.bassDb, 1) << R"(,"level":)" << number (session.currentLevelDb(), 1)
         << R"(,"maxLevel":)" << number (o.type == monitoreq::Type::speaker ? MixMeasure::kMaxNoiseDb : MixMonitorEqSession::kMaxInEarLevelDb, 1)
         << R"(,"sides":)" << (int) o.sides << R"(,"positions":)" << session.currentPositions()
         << R"(,"strength":)" << number (o.strength, 2) << R"(,"useFile":)" << flag (o.useFile)
         << R"(,"hasFile":)" << flag (o.hasFile) << R"(,"fileName":)" << jsonString (o.fileName) << "}";

    json << R"(,"targets":[)";
    bool first = true;
    for (auto t : monitoreq::targetsFor (o.type))
    {
        json << (first ? "" : ",") << "[" << (int) t << "," << jsonString (monitoreq::targetName (t)) << "]";
        first = false;
    }
    json << R"(],"mics":[)";
    for (int i = 0; i < 4; ++i)
        json << (i > 0 ? "," : "") << jsonString (monitoreq::micName ((monitoreq::Mic) i));
    json << "]";

    // The bands: as the suggestion has them (live while measuring), or the target alone.
    const auto& s = session.getSuggestion();
    auto targetDb = monitoreq::target (session.currentTarget(), o.bassDb);
    const auto at1k = targetDb[17];
    for (auto& v : targetDb)
        v -= at1k;
    const auto hasData = std::any_of (s.noData.begin(), s.noData.end(), [] (bool v) { return ! v; });
    json << R"(,"t":)" << bands (hasData ? s.targetDb : targetDb);
    if (hasData)
        json << R"(,"n":)" << bands (s.normalised) << R"(,"c":)" << bands (s.correction) << R"(,"g":)" << bands (s.gains)
             << R"(,"nd":)" << flags (s.noData) << R"(,"lc":)" << flags (s.lowConfidence) << R"(,"nl":)" << flags (s.null)
             << R"(,"or":)" << flags (s.outOfRange);
    return json + "}";
}

//==============================================================================
juce::String MixConsole::typesJson() const
{
    juce::String json = R"({"type":"types","list":[)";
    bool first = true;
    for (const auto& t : rkfx::pedals::pedalTypes())
    {
        json << (first ? "" : ",") << R"({"n":)" << jsonString (t.name) << R"(,"s":)" << jsonString (t.shortName)
             << R"(,"f":")" << hexOf (rkfx::Palette::familyColour (t.family)) << R"(","fam":)"
             << jsonString (rkfx::pedals::familyName (t.family)) << R"(,"midi":)" << flag (t.sendsMidi) << "}";
        first = false;
    }
    return json + "]}";
}

void MixConsole::insertCommand (Client& client, const juce::String& type, const juce::var& m)
{
    // The slot: "s" (ch3, bus2, main, mono) and "n" (0-based).
    const auto strip = m["s"].toString();
    InsertRack::Slot slot;
    if (const auto ch = indexAfter (strip, "ch", kChannels); ch >= 0)
        slot = { InsertRack::Strip::channel, ch, 0 };
    else if (const auto bus = indexAfter (strip, "bus", kBuses); bus >= 0)
        slot = { InsertRack::Strip::bus, bus, 0 };
    else if (strip == "main" || strip == "mono")
        slot = { strip == "main" ? InsertRack::Strip::main : InsertRack::Strip::mono, 0, 0 };
    else
        return;
    if (slot.strip == InsertRack::Strip::channel && slot.index % 2 == 1 && mixer.isLinked (slot.index / 2))
        return;   // a linked pair's inserts are its left channel's
    slot.slot = (int) m["n"];
    if (! juce::isPositiveAndBelow (slot.slot, InsertRack::numSlots (slot.strip)))
        return;

    auto& rack = mixer.getRack();
    const auto flat = InsertRack::flatIndex (slot);
    if (type == "insAdd")
    {
        if (const auto* t = rkfx::pedals::findPedalType (m["p"].toString()))
            rack.insert (slot, t->name, mixer.channelsOf (slot.strip, slot.index));
    }
    else if (type == "insRemove")
        rack.remove (slot);
    else if (type == "insOn" && rack.get (slot) != nullptr)
        rack.setOn (slot, (bool) m["on"]);
    else if (type == "insCopy")
    {
        client.clipboard = mixer.copyInsert (slot);
        post (client, R"({"type":"clip","kind":"insert","text":)" + jsonString (RKMixProcessor::isInsertCopy (client.clipboard)
                                                                 ? RKMixProcessor::describeCopy (client.clipboard) : juce::String()) + "}");
    }
    else if (type == "insPaste" && RKMixProcessor::isInsertCopy (client.clipboard))
        mixer.pasteInsert (slot, client.clipboard);
    else if (type == "insParams")
    {
        // The insert's controls, for the generic list: ID, name, kind.
        juce::String json;
        json << R"({"type":"insParams","s":)" << jsonString (strip) << R"(,"n":)" << slot.slot << R"(,"slot":)" << flat;
        if (auto* plugin = rack.get (slot))
        {
            json << R"(,"p":)" << jsonString (rack.typeOf (slot) != nullptr ? rack.typeOf (slot)->name : juce::String()) << R"(,"params":[)";
            bool first = true;
            for (auto* param : plugin->getParameters())
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param); p != nullptr && p->getParameterID() != "bypass")
                {
                    const auto kind = dynamic_cast<juce::AudioParameterBool*> (p) != nullptr ? "bool"
                                    : dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr ? "choice" : "float";
                    json << (first ? "" : ",") << R"({"id":)" << jsonString ("ins:" + juce::String (flat) + ":" + p->getParameterID())
                         << R"(,"name":)" << jsonString (p->getName (40)) << R"(,"k":")" << kind << "\"}";
                    first = false;
                }
            json << "]";
        }
        post (client, json + "}");
    }
}

//==============================================================================
juce::String MixConsole::metersJson (const MixRemoteMeters& m)
{
    juce::String json;
    const auto db = [] (float gain) { return number (juce::Decibels::gainToDecibels (gain, -100.0f), 1); };
    const auto list = [&json] (const char* name, auto begin, auto end, auto convert)
    {
        json << ",\"" << name << "\":[";
        for (auto it = begin; it != end; ++it)
            json << (it != begin ? "," : "") << convert (*it);
        json << "]";
    };
    const auto reduction = [] (float v) { return number (juce::jmax (-60.0f, v), 1); };

    json << R"({"type":"m")";
    list ("c", m.channels.begin(), m.channels.end(), db);
    json << ",\"b\":[";
    for (size_t bus = 0; bus < m.buses.size(); ++bus)
        json << (bus > 0 ? "," : "") << db (m.buses[bus][0]) << "," << db (m.buses[bus][1]);
    json << "],\"mn\":[" << db (m.main[0]) << "," << db (m.main[1]) << "],\"mo\":" << db (m.mono);
    list ("g", m.gate.begin(), m.gate.end(), reduction);
    list ("k", m.comp.begin(), m.comp.end(), reduction);
    list ("cc", m.chainComp.begin(), m.chainComp.end(), reduction);
    list ("cl", m.chainLimit.begin(), m.chainLimit.end(), reduction);
    return json + "}";
}
