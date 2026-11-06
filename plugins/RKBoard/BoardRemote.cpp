#include "BoardRemote.h"

#include "BoardProcessor.h"
#include "RKBoardRemoteFiles.h"
#include "gui/LookAndFeel.h"
#include "plugin/PresetLibrary.h"

#include <cmath>
#include <unistd.h>

namespace rkboard
{

namespace
{
    constexpr int kTag = 2000;   // a board page's connection

    juce::String jsonString (const juce::String& s) { return juce::JSON::toString (juce::var (s)); }

    juce::String number (float v, int decimals)
    {
        if (! std::isfinite (v))
            return "0";
        auto s = juce::String (v, decimals);
        if (s.containsChar ('.'))
            s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s == "-0" ? juce::String ("0") : s;
    }

    juce::String hexOf (juce::Colour c) { return "#" + c.toDisplayString (false).toLowerCase(); }

    const char* findFile (const juce::String& name, int& size)
    {
       #ifdef RKBOARD_REMOTE_FROM_DISK
        // The remote demo tool: the pages as they are on disk.
        {
            static juce::CriticalSection cacheLock;
            static std::map<juce::String, juce::MemoryBlock> cache;
            const juce::ScopedLock sl (cacheLock);
            auto file = juce::File (RKBOARD_REMOTE_FROM_DISK).getChildFile (name);
            if (! file.existsAsFile())
                file = juce::File (RKBOARD_REMOTE_FROM_DISK).getChildFile ("../../../host/remote").getChildFile (name);
            if (file.existsAsFile())
            {
                auto& block = cache[name];
                block.reset();
                file.loadFileAsData (block);
                size = (int) block.getSize();
                return static_cast<const char*> (block.getData());
            }
        }
       #endif
        for (int i = 0; i < RKBoardRemoteFiles::namedResourceListSize; ++i)
            if (name == RKBoardRemoteFiles::originalFilenames[i])
                return RKBoardRemoteFiles::getNamedResource (RKBoardRemoteFiles::namedResourceList[i], size);
        size = 0;
        return nullptr;
    }

    /** The home-screen icon: the board's accent, three footswitches. */
    juce::MemoryBlock makeIcon()
    {
        constexpr int size = 180;
        juce::Image image (juce::Image::ARGB, size, size, true);
        {
            juce::Graphics g (image);
            g.setColour (juce::Colour (0xff26282c));
            g.fillRect (0, 0, size, size);
            const juce::Colour colours[] = { juce::Colour (0xffd4403a), juce::Colour (0xff3dae5b), juce::Colour (0xff3a9ad9) };
            for (int i = 0; i < 3; ++i)
            {
                const auto x = 22.0f + (float) i * 50.0f;
                g.setColour (colours[i].interpolatedWith (juce::Colour (0xff34373c), 0.35f));
                g.fillRoundedRectangle (x, 40.0f, 40.0f, 100.0f, 6.0f);
                g.setColour (colours[i]);
                g.fillRoundedRectangle (x, 40.0f, 40.0f, 8.0f, 3.0f);
                g.setColour (juce::Colour (0xffe6e2d8));
                g.fillEllipse (x + 8.0f, 102.0f, 24.0f, 24.0f);
            }
        }
        juce::MemoryOutputStream out;
        juce::PNGImageFormat().writeImageToStream (image, out);
        return out.getMemoryBlock();
    }
}

//==============================================================================
struct BoardRemoteHub::Client
{
    rkfx::RemoteServer::Connection* connection = nullptr;
    int id = 0;
    juce::String address, name, token;
    bool signedIn = false;
    juce::StringArray inbox, outbox;
    int boardId = 0;
    int shownPedal = 0;
    struct Sub
    {
        juce::String id;
        float last = -1.0f;
        juce::String lastText;
        bool described = false;
        juce::RangedAudioParameter* param = nullptr;
    };
    std::vector<Sub> subs;
    juce::String stateSent, controlsSent, boardsSent;
};

//==============================================================================
// Sends what waits for each page: woken when something is queued, else every 50 ms.
class BoardRemoteHub::Pusher : public juce::Thread
{
public:
    explicit Pusher (BoardRemoteHub& o) : juce::Thread ("RKBoard remote push"), owner (o) {}
    ~Pusher() override { stopThread (2000); }

    void run() override
    {
        while (! threadShouldExit())
        {
            wait (50);
            owner.server.forEachConnection ([this] (rkfx::RemoteServer::Connection& c)
            {
                if (c.tag == kTag)
                    owner.send (c);
            });
        }
    }

private:
    BoardRemoteHub& owner;
};

//==============================================================================
BoardRemoteHub::BoardRemoteHub()
{
    settingsFile = rkfx::PresetLibrary::defaultSettingsFile().getParentDirectory().getChildFile ("RKBoard").getChildFile ("remote.json");
    icon = makeIcon();

    server.onHttp = [this] (const juce::String& path) { return http (path); };
    server.onOpen = [this] (rkfx::RemoteServer::Connection& c)
    {
        if (c.path != "/ws/board" || ! enabled)
            return false;
        c.tag = kTag;
        opened (c);
        return true;
    };
    server.onMessage = [this] (rkfx::RemoteServer::Connection& c, const juce::String& message)
    {
        if (c.tag == kTag)
            received (c, message);
    };
    server.onClose = [this] (rkfx::RemoteServer::Connection& c)
    {
        if (c.tag == kTag)
            closed (c);
        c.tag = -1;
    };

    loadSettings();
}

BoardRemoteHub::~BoardRemoteHub()
{
    stopTimer();
    pusher.reset();
    server.stop();
}

void BoardRemoteHub::setSettingsFileForTesting (const juce::File& f)
{
    settingsFile = f;
    loadSettings();
    restart();
}

//==============================================================================
void BoardRemoteHub::addBoard (BoardProcessor& processor, bool serve)
{
    {
        const juce::ScopedLock sl (lock);
        auto board = std::make_unique<Board>();
        board->processor = &processor;
        board->id = nextBoardId++;
        boards.push_back (std::move (board));
    }
    ++version;
    if (serve && ! serving)
    {
        serving = true;
        restart();
        startTimerHz (20);
    }
}

void BoardRemoteHub::removeBoard (BoardProcessor& processor)
{
    const juce::ScopedLock sl (lock);
    boards.erase (std::remove_if (boards.begin(), boards.end(), [&] (const auto& b) { return b->processor == &processor; }), boards.end());
    ++version;
}

int BoardRemoteHub::idOf (const BoardProcessor& processor) const
{
    const juce::ScopedLock sl (lock);
    for (const auto& b : boards)
        if (b->processor == &processor)
            return b->id;
    return 0;
}

BoardRemoteHub::Board* BoardRemoteHub::findBoard (int id) const
{
    for (const auto& b : boards)
        if (b->id == id)
            return b.get();
    return nullptr;
}

//==============================================================================
void BoardRemoteHub::loadSettings()
{
    juce::var root;
    if (! juce::JSON::parse (settingsFile.loadFileAsString(), root).wasOk())
        root = juce::var();
    const juce::ScopedLock sl (lock);
    enabled = (bool) root.getProperty ("enabled", false);
    port = juce::jlimit (1024, 65535, (int) root.getProperty ("port", kDefaultPort));
    useIp = (bool) root.getProperty ("useIp", false);
    pin = root.getProperty ("pin", {}).toString();
    known.clear();
    if (const auto* devices = root.getProperty ("devices", {}).getArray())
        for (const auto& d : *devices)
            if (const auto token = d.getProperty ("token", {}).toString(); token.length() >= 16)
                known.push_back ({ token, d.getProperty ("name", {}).toString(), (juce::int64) d.getProperty ("lastSeen", 0) });
    if (pin.length() < 4 || pin.length() > 8 || ! pin.containsOnly ("0123456789"))
    {
        juce::String fresh;
        for (int i = 0; i < 4; ++i)
            fresh << juce::Random::getSystemRandom().nextInt (10);
        pin = fresh;
        known.clear();
        saveSettings();   // (the lock is re-entrant)
    }
}

void BoardRemoteHub::saveSettings() const
{
    auto* root = new juce::DynamicObject();
    {
        const juce::ScopedLock sl (lock);
        root->setProperty ("enabled", enabled);
        root->setProperty ("port", port);
        root->setProperty ("useIp", useIp);
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
    }
    settingsFile.getParentDirectory().createDirectory();
    rkfx::PresetLibrary::writeAtomically (settingsFile, juce::JSON::toString (juce::var (root)));
}

void BoardRemoteHub::settingsChanged()
{
    saveSettings();
    ++version;
}

void BoardRemoteHub::restart()
{
    pusher.reset();
    server.stop();
    portTaken = false;
    if (enabled && serving)
    {
        if (server.start (port))
        {
            pusher = std::make_unique<Pusher> (*this);
            pusher->startThread();
        }
        else
            portTaken = true;
    }
    ++version;
}

void BoardRemoteHub::setEnabled (bool on)
{
    if (on == enabled && on == server.isRunning())
        return;
    enabled = on;
    settingsChanged();
    restart();
}

void BoardRemoteHub::setPort (int p)
{
    p = juce::jlimit (1024, 65535, p);
    if (p == port)
        return;
    port = p;
    settingsChanged();
    restart();
}

void BoardRemoteHub::setLinksUseIpAddress (bool on)
{
    useIp = on;
    settingsChanged();
}

juce::String BoardRemoteHub::getPin() const
{
    const juce::ScopedLock sl (lock);
    return pin;
}

bool BoardRemoteHub::setPin (const juce::String& newPin)
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

juce::String BoardRemoteHub::hostName() const
{
    if (useIp)
        return juce::IPAddress::getLocalAddress().toString();
    char name[256] {};
    if (::gethostname (name, sizeof (name) - 1) != 0 || name[0] == 0)
        return juce::IPAddress::getLocalAddress().toString();
    juce::String host (name);
    if (! host.containsChar ('.'))
        host << ".local";
    return host;
}

juce::String BoardRemoteHub::getStatus() const
{
    if (! enabled)
        return "Off";
    if (! serving)
        return "On (served by the plugin and the app)";
    if (portTaken)
        return "Port " + juce::String (port) + " is in use (another app?): choose another port";
    return "On at http://" + hostName() + ":" + juce::String (port);
}

juce::String BoardRemoteHub::link (bool withPin) const
{
    return "http://" + hostName() + ":" + juce::String (port) + "/" + (withPin ? "#pin=" + getPin() : juce::String());
}

std::vector<BoardRemoteHub::Device> BoardRemoteHub::getDevices()
{
    const juce::ScopedLock sl (lock);
    std::vector<Device> devices;
    for (auto& [c, client] : clients)
    {
        juce::String boardName;
        if (auto* b = findBoard (client->boardId))
            boardName = b->processor->getRemoteName();
        devices.push_back ({ client->id, client->name, client->address, boardName, client->signedIn });
    }
    std::sort (devices.begin(), devices.end(), [] (const Device& a, const Device& b) { return a.id < b.id; });
    return devices;
}

void BoardRemoteHub::disconnect (int id)
{
    {
        const juce::ScopedLock sl (lock);
        for (auto& [c, client] : clients)
            if (client->id == id)
            {
                const auto token = client->token;
                known.erase (std::remove_if (known.begin(), known.end(), [&] (const KnownDevice& d) { return d.token == token; }), known.end());
                client->signedIn = false;
                client->connection->close();
            }
    }
    settingsChanged();
}

//==============================================================================
rkfx::RemoteServer::Response BoardRemoteHub::http (const juce::String& path) const
{
    using Response = rkfx::RemoteServer::Response;
    if (path == "/icon.png" || path == "/apple-touch-icon.png")
    {
        Response r;
        r.contentType = "image/png";
        r.body = icon;
        return r;
    }
    if (path == "/" || path == "/board" || path == "/index.html")
    {
        int size = 0;
        const auto* data = findFile ("board.html", size);
        return data != nullptr ? Response::text (juce::String::fromUTF8 (data, size)) : Response::notFound();
    }
    if (path.startsWith ("/b/"))
    {
        const auto name = path.fromFirstOccurrenceOf ("/b/", false, false);
        int size = 0;
        const auto* data = name.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789-_.") ? findFile (name, size) : nullptr;
        if (data == nullptr)
            return Response::notFound();
        Response r;
        r.contentType = name.endsWith (".js") ? "text/javascript; charset=utf-8" : "text/html; charset=utf-8";
        r.body.append (data, (size_t) size);
        return r;
    }
    if (path == "/manifest/board.json")
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("name", "RKBoard Remote");
        m->setProperty ("short_name", "RKBoard");
        m->setProperty ("start_url", "/");
        m->setProperty ("scope", "/");
        m->setProperty ("display", "standalone");
        m->setProperty ("background_color", "#1c1d20");
        m->setProperty ("theme_color", "#1c1d20");
        auto* i = new juce::DynamicObject();
        i->setProperty ("src", "/icon.png");
        i->setProperty ("sizes", "180x180");
        i->setProperty ("type", "image/png");
        m->setProperty ("icons", juce::Array<juce::var> { juce::var (i) });
        return Response::text (juce::JSON::toString (juce::var (m)), "application/manifest+json");
    }
    return Response::notFound();
}

//==============================================================================
void BoardRemoteHub::opened (rkfx::RemoteServer::Connection& c)
{
    {
        const juce::ScopedLock sl (lock);
        auto client = std::make_unique<Client>();
        client->connection = &c;
        client->id = nextClientId++;
        client->address = c.address;
        clients[&c] = std::move (client);
    }
    ++version;
}

void BoardRemoteHub::closed (rkfx::RemoteServer::Connection& c)
{
    {
        const juce::ScopedLock sl (lock);
        clients.erase (&c);
    }
    ++version;
}

void BoardRemoteHub::received (rkfx::RemoteServer::Connection& c, const juce::String& message)
{
    const juce::ScopedLock sl (lock);
    if (auto it = clients.find (&c); it != clients.end() && it->second->inbox.size() < 2000)
        it->second->inbox.add (message);
}

void BoardRemoteHub::send (rkfx::RemoteServer::Connection& c)
{
    juce::StringArray out;
    {
        const juce::ScopedLock sl (lock);
        if (auto it = clients.find (&c); it != clients.end())
            out.swapWith (it->second->outbox);
    }
    for (const auto& text : out)
        c.send (text);
}

void BoardRemoteHub::post (Client& client, const juce::String& text)
{
    if (client.outbox.size() > 500)   // a device that stopped reading: let it go
    {
        client.connection->close();
        return;
    }
    client.outbox.add (text);
}

//==============================================================================
void BoardRemoteHub::tick()
{
    bool posted = false;
    {
        const juce::ScopedLock sl (lock);
        const auto list = clients.empty() ? juce::String() : boardsJson();
        std::map<int, juce::String> states;   // by board, made once for all its pages

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
                if (client.boardsSent != list)
                {
                    client.boardsSent = list;
                    post (client, list);
                }
                if (auto* board = findBoard (client.boardId))
                {
                    if (states.count (board->id) == 0)
                    {
                        updateTuner (*board);
                        states[board->id] = stateJson (*board->processor, 0);
                    }
                    if (client.stateSent != states[board->id])
                    {
                        client.stateSent = states[board->id];
                        post (client, client.stateSent);
                    }
                    // The knobs of the pedal the page shows.
                    const auto controls = stateJson (*board->processor, client.shownPedal);
                    if (client.shownPedal != 0 && controls != client.controlsSent)
                    {
                        client.controlsSent = controls;
                        post (client, controls);
                    }
                    queueValues (client, *board->processor);
                }
                else if (client.boardId != 0)
                {
                    client.boardId = 0;   // that board is gone (its rackspace was deleted)
                    client.stateSent.clear();
                    post (client, R"({"type":"gone"})");
                }
            }
            posted = posted || client.outbox.size() != before;
        }
    }
    if (posted && pusher != nullptr)
        pusher->notify();
}

juce::String BoardRemoteHub::boardsJson() const
{
    juce::String json;
    for (const auto& b : boards)
        json << (json.isEmpty() ? "" : ",") << "{\"id\":" << b->id << ",\"name\":" << jsonString (b->processor->getRemoteName())
             << ",\"preset\":" << jsonString (b->processor->getPresetName()) << "}";
    return R"({"type":"boards","list":[)" + json + "]}";
}

void BoardRemoteHub::handle (Client& client, const juce::var& message)
{
    const auto type = message["t"].toString();
    if (type == "hello")
        return signIn (client, message);
    if (! client.signedIn)
        return;

    if (type == "open")
    {
        client.boardId = findBoard ((int) message["b"]) != nullptr ? (int) message["b"] : 0;
        client.stateSent.clear();
        client.controlsSent.clear();
        client.shownPedal = 0;
        for (auto& sub : client.subs)
            sub.described = false;
        return;
    }
    if (type == "sub")
    {
        std::vector<Client::Sub> next;
        if (const auto* list = message["ids"].getArray())
            for (const auto& v : *list)
            {
                const auto id = v.toString();
                if (id.isEmpty() || next.size() >= 500)
                    continue;
                auto it = std::find_if (client.subs.begin(), client.subs.end(), [&] (const Client::Sub& s) { return s.id == id; });
                if (it != client.subs.end()) next.push_back (*it); else { Client::Sub sub; sub.id = id; next.push_back (sub); }
            }
        client.subs = std::move (next);
        return;
    }
    if (auto* board = findBoard (client.boardId))
        command (client, *board, type, message);
}

void BoardRemoteHub::signIn (Client& client, const juce::var& message)
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
    settingsChanged();
}

//==============================================================================
juce::RangedAudioParameter* BoardRemoteHub::resolve (BoardProcessor& board, const juce::String& id) const
{
    if (id == "morph") return &board.getMorphParam();
    if (id == "in")    return &board.getInputParam();
    if (id == "out")   return &board.getOutputParam();
    if (id.startsWith ("a") && id.substring (1).containsOnly ("0123456789"))
    {
        const auto slot = id.substring (1).getIntValue() - 1;
        return juce::isPositiveAndBelow (slot, BoardProcessor::kNumAssigns) ? &board.getAssignParam (slot) : nullptr;
    }
    const auto owner = id.substring (1).upToFirstOccurrenceOf (":", false, false);
    const auto paramId = id.fromFirstOccurrenceOf (":", false, false);
    if (owner.isEmpty() || ! owner.containsOnly ("0123456789") || paramId.isEmpty() || paramId == "bypass")
        return nullptr;   // a pedal's bypass is its footswitch, a command
    if (id.startsWith ("p"))
        return board.findOwnerParam (owner.getIntValue(), paramId);
    if (id.startsWith ("s"))
        return board.findOwnerParam (BoardProcessor::routingOwner (owner.getIntValue() - 1), paramId);
    return nullptr;
}

void BoardRemoteHub::setValue (BoardProcessor& board, const juce::String& id, float normalised)
{
    if (auto* p = resolve (board, id))
        if (std::isfinite (normalised))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

void BoardRemoteHub::command (Client& client, Board& b, const juce::String& type, const juce::var& message)
{
    auto& board = *b.processor;
    if (type == "set")
        setValue (board, message["id"].toString(), (float) message["v"]);
    else if (type == "param")
    {
        const auto owner = (int) message["owner"];
        setValue (board, (owner > 0 ? "p" + juce::String (owner) : "s" + juce::String (-owner)) + ":" + message["id"].toString(),
                  (float) message["v"]);
    }
    else if (type == "assign")
        setValue (board, "a" + juce::String ((int) message["slot"]), (float) message["v"]);
    else if (type == "morph")
        setValue (board, "morph", (float) message["v"]);
    else if (type == "scene")
    {
        if (const auto n = (int) message["n"]; juce::isPositiveAndBelow (n, BoardProcessor::kNumScenes))
            board.setScene (n);
    }
    else if (type == "bypass")
        board.setPedalBypassed ((int) message["id"], ! (bool) message["on"]);
    else if (type == "morphScenes")
        board.setMorphScenes ((int) message["from"], (int) message["to"]);
    else if (type == "show")
    {
        client.shownPedal = board.getPedal ((int) message["id"]) != nullptr ? (int) message["id"] : 0;
        client.controlsSent.clear();
    }
    else if (type == "tap")
    {
        if (board.isStandalone())
            board.tapTempo();
    }
    else if (type == "tuner")
    {
        if (board.isStandalone())
            board.setTunerActive ((bool) message["on"]);
    }
}

void BoardRemoteHub::queueValues (Client& client, BoardProcessor& board)
{
    juce::String json;
    for (auto& sub : client.subs)
    {
        auto* param = resolve (board, sub.id);
        if (param != sub.param)
        {
            sub.param = param;
            sub.described = false;
        }
        if (param == nullptr)
            continue;
        const auto v = param->getValue();
        const auto text = param->getCurrentValueAsText();
        if (sub.described && std::abs (v - sub.last) < 1.0e-6f && text == sub.lastText)
            continue;
        json << (json.isEmpty() ? "" : ",") << jsonString (sub.id) << ":[" << number (v, 6) << "," << jsonString (text);
        if (! sub.described)
        {
            const auto steps = param->getNumSteps();
            json << "," << number (param->getDefaultValue(), 6) << "," << (steps > 1 && steps < 0x7fffffff ? steps : 0);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
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
        sub.lastText = text;
    }
    if (json.isNotEmpty())
        post (client, R"({"type":"v","p":{)" + json + "}}");
}

void BoardRemoteHub::updateTuner (Board& b)
{
    auto& board = *b.processor;
    if (! board.isStandalone() || ! board.isTunerActive())
    {
        b.hz = 0.0f;
        return;
    }
    // Ten times a second is plenty for a phone.
    const auto now = juce::Time::getMillisecondCounter();
    if (now - b.lastTuner < 100)
        return;
    b.lastTuner = now;
    if (b.window.empty())
    {
        b.detector.prepare (board.getSampleRate() > 0.0 ? board.getSampleRate() : 48000.0, 4096);
        b.window.resize ((size_t) b.detector.getWindowSize());
    }
    board.copyTunerInput (b.window.data(), (int) b.window.size());
    b.hz = b.detector.detect (b.window.data());
}

juce::String BoardRemoteHub::stateJson (BoardProcessor& board, int shownPedal) const
{
    // A pedal's knobs (the page shows them when its tile is tapped).
    if (shownPedal != 0)
    {
        auto* pedal = board.getPedal (shownPedal);
        if (pedal == nullptr)
            return {};
        juce::String params;
        for (auto* p : pedal->getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && p != pedal->getBypassParameter())
                params << (params.isEmpty() ? "" : ",") << "{\"id\":" << jsonString ("p" + juce::String (shownPedal) + ":" + ranged->getParameterID())
                       << ",\"n\":" << jsonString (ranged->getName (24)) << "}";
        return R"({"type":"controls","id":)" + juce::String (shownPedal) + ",\"params\":[" + params + "]}";
    }

    const auto* found = [&]() -> const Board*
    {
        for (const auto& b : boards)
            if (b->processor == &board)
                return b.get();
        return nullptr;
    }();

    juce::String json;
    json << R"({"type":"board","id":)" << (found != nullptr ? found->id : 0)
         << ",\"name\":" << jsonString (board.getRemoteName())
         << ",\"preset\":" << jsonString (board.getPresetName())
         << ",\"app\":" << (board.isStandalone() ? "true" : "false")
         << ",\"scene\":" << board.getScene() << ",\"names\":[";
    for (int i = 0; i < BoardProcessor::kNumScenes; ++i)
        json << (i > 0 ? "," : "") << jsonString (board.getSceneName (i));
    json << "],\"morph\":{\"from\":" << board.getMorphFrom() << ",\"to\":" << board.getMorphTo()
         << ",\"on\":" << (board.isMorphing() ? "true" : "false") << "}";

    const auto bpm = board.getPedalTempo();
    json << ",\"tempo\":{\"bpm\":" << number ((float) bpm, 1)
         << ",\"tap\":" << (board.isStandalone() && ! board.isFollowingClock() ? "true" : "false")
         << ",\"clock\":" << (board.isFollowingClock() ? "true" : "false") << "}";

    if (board.isStandalone())
    {
        json << ",\"tuner\":{\"on\":" << (board.isTunerActive() ? "true" : "false");
        if (found != nullptr && found->hz > 0.0f)
        {
            const auto note = PitchDetector::noteFor (found->hz, board.getTunerReference());
            json << ",\"hz\":" << number (found->hz, 2) << ",\"note\":" << jsonString (note.name)
                 << ",\"octave\":" << note.octave << ",\"cents\":" << number (note.cents, 1);
        }
        json << ",\"ref\":" << number (board.getTunerReference(), 1) << "}";
    }

    json << ",\"pedals\":[";
    auto first = true;
    for (int s = 0; s < board.getNumSections(); ++s)
        for (const auto id : board.getSection ((Section) s))
            if (auto* pedal = board.getPedal (id))
            {
                const auto* type = findPedalType (pedal->getConfig().name);
                json << (first ? "" : ",") << "{\"id\":" << id
                     << ",\"n\":" << jsonString (type != nullptr ? type->shortName : pedal->getConfig().name)
                     << ",\"c\":" << jsonString (hexOf (rkfx::Palette::familyColour (pedal->getConfig().family)))
                     << ",\"on\":" << (board.isPedalBypassed (id) ? "false" : "true")
                     << ",\"s\":" << s << "}";
                first = false;
            }
    json << "],\"assign\":[";
    first = true;
    for (int slot = 0; slot < BoardProcessor::kNumAssigns; ++slot)
        if (const auto what = board.describeAssignment (slot); what.isNotEmpty())
        {
            json << (first ? "" : ",") << "{\"slot\":" << (slot + 1) << ",\"n\":" << jsonString (what)
                 << ",\"sw\":" << (board.isSwitchAssignment (slot) ? "true" : "false") << "}";
            first = false;
        }
    json << "]}";
    return json;
}

} // namespace rkboard
