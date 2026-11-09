#include "PlayRemote.h"

#include "PluginProcessor.h"
#include "RKPlayRemoteFiles.h"
#include "plugin/PresetLibrary.h"

#include <cmath>
#include <unistd.h>

namespace
{
    constexpr int kTag = 3000;   // a player page's connection

    juce::String jsonString (const juce::String& s) { return juce::JSON::toString (juce::var (s)); }

    juce::String number (double v, int decimals)
    {
        if (! std::isfinite (v))
            return "0";
        auto s = juce::String (v, decimals);
        if (s.containsChar ('.'))
            s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s == "-0" ? juce::String ("0") : s;
    }

    const char* bool01 (bool b) { return b ? "true" : "false"; }

    juce::String clockTime (int minutes)
    {
        return juce::String (minutes / 60).paddedLeft ('0', 2) + ":" + juce::String (minutes % 60).paddedLeft ('0', 2);
    }

    const char* findFile (const juce::String& name, int& size)
    {
       #ifdef RKPLAY_REMOTE_FROM_DISK
        // The remote demo tool: the pages as they are on disk.
        {
            static juce::CriticalSection cacheLock;
            static std::map<juce::String, juce::MemoryBlock> cache;
            const juce::ScopedLock sl (cacheLock);
            auto file = juce::File (RKPLAY_REMOTE_FROM_DISK).getChildFile (name);
            if (! file.existsAsFile())
                file = juce::File (RKPLAY_REMOTE_FROM_DISK).getChildFile ("../../../host/remote").getChildFile (name);
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
        for (int i = 0; i < RKPlayRemoteFiles::namedResourceListSize; ++i)
            if (name == RKPlayRemoteFiles::originalFilenames[i])
                return RKPlayRemoteFiles::getNamedResource (RKPlayRemoteFiles::namedResourceList[i], size);
        size = 0;
        return nullptr;
    }

    /** The home-screen icon: a play triangle over three song rows. */
    juce::MemoryBlock makeIcon()
    {
        constexpr int size = 180;
        juce::Image image (juce::Image::ARGB, size, size, true);
        {
            juce::Graphics g (image);
            g.setColour (juce::Colour (0xff26282c));
            g.fillRect (0, 0, size, size);
            g.setColour (juce::Colour (0xff4a4e55));
            for (int i = 0; i < 3; ++i)
                g.fillRoundedRectangle (30.0f, 104.0f + (float) i * 18.0f, 120.0f, 9.0f, 4.0f);
            juce::Path triangle;
            triangle.addTriangle (68.0f, 26.0f, 68.0f, 86.0f, 120.0f, 56.0f);
            g.setColour (juce::Colour (0xffe6e2d8));
            g.fillPath (triangle);
        }
        juce::MemoryOutputStream out;
        juce::PNGImageFormat().writeImageToStream (image, out);
        return out.getMemoryBlock();
    }

    double lengthOf (const PlayShow& show, const PlaySong& s)
    {
        const auto span = show.spanOf (s);
        return s.isTrimmed() ? span.end - span.start : s.lengthSeconds;
    }

    // [title, artist, length, missing, file]
    juce::String songRow (const PlayShow& show, const PlaySong& s)
    {
        return "[" + jsonString (s.title) + "," + jsonString (s.artist) + "," + number (lengthOf (show, s), 1) + ","
             + (s.isPlayable() ? "0" : "1") + "," + jsonString (s.file.getFullPathName()) + "]";
    }
}

//==============================================================================
struct PlayRemoteHub::Client
{
    rkfx::RemoteServer::Connection* connection = nullptr;
    int id = 0;
    juce::String address, name, token;
    bool signedIn = false;
    juce::StringArray inbox, outbox;
    int playerId = 0;
    int songsPlaylist = -1;            // the list the page shows (-1: none)
    juce::String playersSent, nowSent, listsSent, queueSent, padsSent;
    juce::int64 songsSent = 0;         // the hash of the song list last sent
};

//==============================================================================
// Sends what waits for each page: woken when something is queued, else every 50 ms.
class PlayRemoteHub::Pusher : public juce::Thread
{
public:
    explicit Pusher (PlayRemoteHub& o) : juce::Thread ("RKPlay remote push"), owner (o) {}
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
    PlayRemoteHub& owner;
};

//==============================================================================
PlayRemoteHub::PlayRemoteHub()
{
    settingsFile = rkfx::PresetLibrary::defaultSettingsFile().getParentDirectory().getChildFile ("RKPlay").getChildFile ("remote.json");
    icon = makeIcon();

    server.onHttp = [this] (const juce::String& path) { return http (path); };
    server.onOpen = [this] (rkfx::RemoteServer::Connection& c)
    {
        if (c.path != "/ws/play" || ! enabled)
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

PlayRemoteHub::~PlayRemoteHub()
{
    stopTimer();
    pusher.reset();
    server.stop();
}

void PlayRemoteHub::setSettingsFileForTesting (const juce::File& f)
{
    settingsFile = f;
    loadSettings();
    restart();
}

//==============================================================================
void PlayRemoteHub::addPlayer (RKPlayProcessor& processor, bool serve)
{
    {
        const juce::ScopedLock sl (lock);
        auto player = std::make_unique<Player>();
        player->processor = &processor;
        player->id = nextPlayerId++;
        players.push_back (std::move (player));
    }
    ++version;
    if (serve && ! serving)
    {
        serving = true;
        restart();
        startTimerHz (20);
    }
}

void PlayRemoteHub::removePlayer (RKPlayProcessor& processor)
{
    const juce::ScopedLock sl (lock);
    players.erase (std::remove_if (players.begin(), players.end(), [&] (const auto& p) { return p->processor == &processor; }), players.end());
    ++version;
}

int PlayRemoteHub::idOf (const RKPlayProcessor& processor) const
{
    const juce::ScopedLock sl (lock);
    for (const auto& p : players)
        if (p->processor == &processor)
            return p->id;
    return 0;
}

PlayRemoteHub::Player* PlayRemoteHub::findPlayer (int id) const
{
    for (const auto& p : players)
        if (p->id == id)
            return p.get();
    return nullptr;
}

//==============================================================================
// Settings, the PIN and the devices: as RKBoard's remote (BoardRemote.cpp).

void PlayRemoteHub::loadSettings()
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

void PlayRemoteHub::saveSettings() const
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

void PlayRemoteHub::settingsChanged()
{
    saveSettings();
    ++version;
}

void PlayRemoteHub::restart()
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

void PlayRemoteHub::setEnabled (bool on)
{
    if (on == enabled && on == server.isRunning())
        return;
    enabled = on;
    settingsChanged();
    restart();
}

void PlayRemoteHub::setPort (int p)
{
    p = juce::jlimit (1024, 65535, p);
    if (p == port)
        return;
    port = p;
    settingsChanged();
    restart();
}

void PlayRemoteHub::setLinksUseIpAddress (bool on)
{
    useIp = on;
    settingsChanged();
}

juce::String PlayRemoteHub::getPin() const
{
    const juce::ScopedLock sl (lock);
    return pin;
}

bool PlayRemoteHub::setPin (const juce::String& newPin)
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

juce::String PlayRemoteHub::hostName() const
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

juce::String PlayRemoteHub::getStatus() const
{
    if (! enabled)
        return "Off";
    if (! serving)
        return "On (served by the plugin and the app)";
    if (portTaken)
        return "Port " + juce::String (port) + " is in use (another app?): choose another port";
    return "On at http://" + hostName() + ":" + juce::String (port);
}

juce::String PlayRemoteHub::link (bool withPin) const
{
    return "http://" + hostName() + ":" + juce::String (port) + "/" + (withPin ? "#pin=" + getPin() : juce::String());
}

std::vector<PlayRemoteHub::Device> PlayRemoteHub::getDevices()
{
    const juce::ScopedLock sl (lock);
    std::vector<Device> devices;
    for (auto& [c, client] : clients)
    {
        juce::String playerName;
        if (auto* p = findPlayer (client->playerId))
            playerName = p->processor->getPresetName();
        devices.push_back ({ client->id, client->name, client->address, playerName, client->signedIn });
    }
    std::sort (devices.begin(), devices.end(), [] (const Device& a, const Device& b) { return a.id < b.id; });
    return devices;
}

int PlayRemoteHub::numSignedIn()
{
    const juce::ScopedLock sl (lock);
    int n = 0;
    for (auto& [c, client] : clients)
        n += client->signedIn ? 1 : 0;
    return n;
}

void PlayRemoteHub::disconnect (int id)
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
rkfx::RemoteServer::Response PlayRemoteHub::http (const juce::String& path) const
{
    using Response = rkfx::RemoteServer::Response;
    if (path == "/icon.png" || path == "/apple-touch-icon.png")
    {
        Response r;
        r.contentType = "image/png";
        r.body = icon;
        return r;
    }
    if (path == "/" || path == "/play" || path == "/index.html")
    {
        int size = 0;
        const auto* data = findFile ("play.html", size);
        return data != nullptr ? Response::text (juce::String::fromUTF8 (data, size)) : Response::notFound();
    }
    if (path.startsWith ("/p/"))
    {
        const auto name = path.fromFirstOccurrenceOf ("/p/", false, false);
        int size = 0;
        const auto* data = name.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789-_.") ? findFile (name, size) : nullptr;
        if (data == nullptr)
            return Response::notFound();
        Response r;
        r.contentType = name.endsWith (".js") ? "text/javascript; charset=utf-8" : "text/html; charset=utf-8";
        r.body.append (data, (size_t) size);
        return r;
    }
    if (path == "/manifest/play.json")
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("name", "RKPlay Remote");
        m->setProperty ("short_name", "RKPlay");
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
void PlayRemoteHub::opened (rkfx::RemoteServer::Connection& c)
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

void PlayRemoteHub::closed (rkfx::RemoteServer::Connection& c)
{
    {
        const juce::ScopedLock sl (lock);
        clients.erase (&c);
    }
    ++version;
}

void PlayRemoteHub::received (rkfx::RemoteServer::Connection& c, const juce::String& message)
{
    const juce::ScopedLock sl (lock);
    if (auto it = clients.find (&c); it != clients.end() && it->second->inbox.size() < 2000)
        it->second->inbox.add (message);
}

void PlayRemoteHub::send (rkfx::RemoteServer::Connection& c)
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

void PlayRemoteHub::post (Client& client, const juce::String& text)
{
    if (client.outbox.size() > 500)   // a device that stopped reading: let it go
    {
        client.connection->close();
        return;
    }
    client.outbox.add (text);
}

//==============================================================================
void PlayRemoteHub::tick()
{
    bool posted = false;
    {
        const juce::ScopedLock sl (lock);
        const auto list = clients.empty() ? juce::String() : playersJson();
        struct Made { juce::String now, lists, queue, pads; };
        std::map<int, Made> made;   // by player, made once for all its pages

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
                if (client.playersSent != list)
                {
                    client.playersSent = list;
                    post (client, list);
                }
                if (auto* player = findPlayer (client.playerId))
                {
                    auto& p = *player->processor;
                    if (made.count (player->id) == 0)
                        made[player->id] = { nowJson (p), listsJson (p), queueJson (p), padsJson (p) };
                    const auto& m = made[player->id];
                    const auto sendIfChanged = [&] (juce::String& sent, const juce::String& now)
                    {
                        if (sent != now)
                        {
                            sent = now;
                            post (client, now);
                        }
                    };
                    sendIfChanged (client.listsSent, m.lists);
                    sendIfChanged (client.nowSent, m.now);
                    sendIfChanged (client.queueSent, m.queue);
                    sendIfChanged (client.padsSent, m.pads);
                    if (client.songsPlaylist >= 0)
                    {
                        const auto& songs = songsOf (*player, client.songsPlaylist);
                        const auto hash = songs.hashCode64();
                        if (hash != client.songsSent)
                        {
                            client.songsSent = hash;
                            post (client, songs);
                        }
                    }
                }
                else if (client.playerId != 0)
                {
                    client.playerId = 0;   // that player is gone (its rackspace was deleted)
                    client.nowSent.clear();
                    post (client, R"({"type":"gone"})");
                }
            }
            posted = posted || client.outbox.size() != before;
        }
    }
    if (posted && pusher != nullptr)
        pusher->notify();
}

juce::String PlayRemoteHub::playersJson() const
{
    juce::String json;
    for (const auto& p : players)
    {
        const auto name = p->processor->getPresetName();
        json << (json.isEmpty() ? "" : ",") << "{\"id\":" << p->id << ",\"name\":" << jsonString (name.isNotEmpty() ? name : juce::String ("RKPlay")) << "}";
    }
    return R"({"type":"players","list":[)" + json + "]}";
}

void PlayRemoteHub::handle (Client& client, const juce::var& message)
{
    const auto type = message["t"].toString();
    if (type == "hello")
        return signIn (client, message);
    if (! client.signedIn)
        return;

    if (type == "open")
    {
        client.playerId = findPlayer ((int) message["p"]) != nullptr ? (int) message["p"] : 0;
        client.nowSent.clear();
        client.listsSent.clear();
        client.queueSent.clear();
        client.padsSent.clear();
        client.songsSent = 0;
        client.songsPlaylist = -1;
        return;
    }
    if (auto* player = findPlayer (client.playerId))
        command (client, *player, type, message);
}

void PlayRemoteHub::signIn (Client& client, const juce::var& message)
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
int PlayRemoteHub::findSong (const RKPlayProcessor& player, int playlist, int index, const juce::String& file)
{
    const auto& show = player.getShow();
    if (! juce::isPositiveAndBelow (playlist, (int) show.playlists.size()) || file.isEmpty())
        return -1;
    const auto& songs = show.playlists[(size_t) playlist].songs;
    if (juce::isPositiveAndBelow (index, (int) songs.size()) && songs[(size_t) index].file.getFullPathName() == file)
        return index;
    for (int i = 0; i < (int) songs.size(); ++i)
        if (songs[(size_t) i].file.getFullPathName() == file)
            return i;
    return -1;
}

void PlayRemoteHub::command (Client& client, Player& player, const juce::String& type, const juce::var& message)
{
    auto& p = *player.processor;
    const auto& show = p.getShow();

    if (type == "play")             p.play();
    else if (type == "pause")       p.pause();
    else if (type == "playpause")   p.togglePlay();
    else if (type == "next")        p.next();
    else if (type == "prev")        p.previous();
    else if (type == "fade")        p.fadeOut();
    else if (type == "stop")        p.stop();
    else if (type == "panic")       p.panic();
    else if (type == "level")
    {
        const auto v = (float) (double) message["v"];
        if (auto* param = p.findParam ("level"); param != nullptr && std::isfinite (v))
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    }
    else if (type == "duck")        p.setManualDuck ((bool) message["on"]);
    else if (type == "pad")
    {
        if (const auto n = (int) message["n"]; juce::isPositiveAndBelow (n, PlayEngine::kPads))
            p.triggerPad (n);
    }
    else if (type == "switch")
    {
        if (const auto n = (int) message["playlist"]; juce::isPositiveAndBelow (n, (int) show.playlists.size()))
            p.switchPlaylist (n, (bool) message["now"]);
    }
    else if (type == "cancelSwitch")
    {
        if (p.getPendingSwitch() >= 0)
            p.switchPlaylist (show.active, false);   // the playing list again: no switch waits
    }
    else if (type == "queueNext" || type == "playNow" || type == "jump")
    {
        const auto playlist = (int) message["playlist"];
        const auto index = findSong (p, playlist, (int) message["index"], message["file"].toString());
        if (index < 0)
        {
            post (client, R"({"type":"moved"})");
            return;
        }
        if (type == "queueNext")    p.queueNext (playlist, index);
        else if (type == "playNow") p.playNow (playlist, index);
        else                        p.jumpTo (playlist, index);
    }
    else if (type == "queueMove" || type == "queueRemove")
    {
        const auto& queue = p.getQueue();
        const auto positionOf = [&] (int id)
        {
            for (int i = 0; i < (int) queue.size(); ++i)
                if (queue[(size_t) i].queueId == id)
                    return i;
            return -1;
        };
        const auto from = positionOf ((int) message["id"]);
        if (from < 0)
            return;
        if (type == "queueRemove")
            p.removeQueued (from);
        else
            p.moveQueued (from, juce::jlimit (0, (int) queue.size() - 1, (int) message["to"]));
    }
    else if (type == "queueClear")  p.clearQueue();
    else if (type == "endAt")
    {
        const auto& endAt = show.endAt;
        if ((bool) message["off"])
            p.setEndAt (false, endAt.minutes, endAt.atSongEnd);
        else
        {
            const auto t = juce::StringArray::fromTokens (message["time"].toString(), ":", {});
            if (t.size() == 2 && t[0].trim().containsOnly ("0123456789") && t[1].trim().containsOnly ("0123456789"))
                p.setEndAt (true, juce::jlimit (0, 23, t[0].getIntValue()) * 60 + juce::jlimit (0, 59, t[1].getIntValue()), endAt.atSongEnd);
        }
    }
    else if (type == "songs")
    {
        const auto n = (int) message["playlist"];
        client.songsPlaylist = juce::isPositiveAndBelow (n, (int) show.playlists.size()) ? n : -1;
        client.songsSent = 0;
    }
    else if (type == "search")
        post (client, searchJson (p, message["q"].toString()));
}

//==============================================================================
juce::String PlayRemoteHub::nowJson (RKPlayProcessor& p) const
{
    using State = RKPlayProcessor::State;
    const auto& show = p.getShow();
    const auto state = p.getState();
    const auto& item = p.getCurrent();
    const auto stopped = state == State::stopped || ! item.isValid();
    const auto nextItem = p.peekNext();
    const auto& song = ! stopped ? item.song : nextItem.song;

    // As the window and the OSC remote: an end set by hand is where the song ends.
    const auto span = show.spanOf (song);
    const auto endsAt = song.hasEnd() ? span.end : song.lengthSeconds;
    const auto position = stopped ? 0.0 : p.getPositionSeconds();
    const auto length = stopped && song.isTrimmed() ? span.end - span.start : endsAt;

    juce::String json;
    json << R"({"type":"now","state":")" << (state == State::playing ? "playing" : state == State::paused ? "paused" : "stopped") << "\""
         << ",\"title\":" << jsonString (song.title) << ",\"artist\":" << jsonString (song.artist)
         << ",\"pos\":" << number (std::floor (position * 4.0) / 4.0, 2)   // quarter seconds: four messages a second at most
         << ",\"len\":" << number (length, 1)
         << ",\"next\":" << jsonString (! stopped && nextItem.isValid() ? nextItem.song.displayName() : juce::String())
         << ",\"active\":" << show.active << ",\"list\":" << jsonString (show.current().name);
    if (! stopped)
        json << ",\"playing\":{\"playlist\":" << item.playlist << ",\"index\":" << item.index << ",\"queued\":" << bool01 (item.queued) << "}";
    json << ",\"cursor\":" << show.current().cursor;

    const auto duckDb = p.getDuckDb();
    json << ",\"duck\":" << bool01 (p.isManualDuck()) << ",\"ducking\":" << bool01 (p.isManualDuck() || duckDb < -0.5f)
         << ",\"duckDb\":" << juce::roundToInt (duckDb);
    if (auto* level = p.findParam ("level"))
    {
        const auto db = level->convertFrom0to1 (level->getValue());
        json << ",\"level\":" << number (level->getValue(), 4)
             << ",\"levelText\":" << jsonString (db <= -59.95f ? juce::String ("Off") : juce::String (db, 1) + " dB");
    }

    if (const auto left = p.getEndAtRemaining(); left >= 0.0)
        json << ",\"endAt\":{\"time\":" << jsonString (clockTime (show.endAt.minutes)) << ",\"left\":" << (int) std::ceil (left)
             << ",\"atSongEnd\":" << bool01 (show.endAt.atSongEnd) << "}";
    if (const auto left = p.getSwitchAtRemaining(); left >= 0.0)
    {
        const auto to = juce::isPositiveAndBelow (show.switchAt.to, (int) show.playlists.size()) ? show.playlists[(size_t) show.switchAt.to].name : juce::String();
        json << ",\"switchAt\":{\"time\":" << jsonString (clockTime (show.switchAt.minutes)) << ",\"left\":" << (int) std::ceil (left)
             << ",\"to\":" << jsonString (to) << "}";
    }
    if (const auto pending = p.getPendingSwitch(); juce::isPositiveAndBelow (pending, (int) show.playlists.size()))
        json << ",\"pending\":" << pending << ",\"pendingName\":" << jsonString (show.playlists[(size_t) pending].name);
    json << ",\"note\":" << jsonString (p.getNote()) << ",\"noteVersion\":" << p.getNoteVersion() << "}";
    return json;
}

juce::String PlayRemoteHub::listsJson (RKPlayProcessor& p) const
{
    const auto& show = p.getShow();
    juce::String json;
    for (const auto& list : show.playlists)
        json << (json.isEmpty() ? "" : ",") << "{\"name\":" << jsonString (list.name) << ",\"count\":" << (int) list.songs.size()
             << ",\"linked\":" << bool01 (list.isLinked()) << "}";
    return R"({"type":"lists","active":)" + juce::String (show.active) + ",\"lists\":[" + json + "]}";
}

juce::String PlayRemoteHub::queueJson (RKPlayProcessor& p) const
{
    const auto& show = p.getShow();
    juce::String json;
    for (const auto& item : p.getQueue())
        json << (json.isEmpty() ? "" : ",") << "{\"id\":" << item.queueId << ",\"title\":" << jsonString (item.song.title)
             << ",\"artist\":" << jsonString (item.song.artist) << ",\"len\":" << number (lengthOf (show, item.song), 1) << "}";
    return R"({"type":"queue","items":[)" + json + "]}";
}

juce::String PlayRemoteHub::padsJson (RKPlayProcessor& p) const
{
    const auto& show = p.getShow();
    juce::String json;
    for (int i = 0; i < PlayEngine::kPads; ++i)
    {
        const auto& pad = show.pads[(size_t) i];
        const auto playing = p.isPadPlaying (i);
        json << (i > 0 ? "," : "") << "{\"name\":" << jsonString (pad.name) << ",\"sound\":" << bool01 (! pad.isEmpty())
             << ",\"playing\":" << bool01 (playing)
             << ",\"progress\":" << (playing ? number (std::floor (p.getPadProgress (i) * 20.0f) / 20.0f, 2) : juce::String ("0")) << "}";
    }
    return R"({"type":"pads","pads":[)" + json + "]}";
}

juce::String PlayRemoteHub::songsJson (RKPlayProcessor& p, int playlist) const
{
    const auto& show = p.getShow();
    if (! juce::isPositiveAndBelow (playlist, (int) show.playlists.size()))
        return {};
    juce::String json;
    json.preallocateBytes (show.playlists[(size_t) playlist].songs.size() * 120 + 64);
    for (const auto& s : show.playlists[(size_t) playlist].songs)
        json << (json.isEmpty() ? "" : ",") << songRow (show, s);
    return R"({"type":"songs","playlist":)" + juce::String (playlist) + ",\"songs\":[" + json + "]}";
}

const juce::String& PlayRemoteHub::songsOf (Player& player, int playlist)
{
    // Made again only when the show changed (an edit, a rescan, a file found or lost).
    if (player.songsVersion != player.processor->getShowVersion())
    {
        player.songsVersion = player.processor->getShowVersion();
        player.songs.clear();
    }
    auto it = player.songs.find (playlist);
    if (it == player.songs.end())
        it = player.songs.emplace (playlist, songsJson (*player.processor, playlist)).first;
    return it->second;
}

//==============================================================================
juce::String PlayRemoteHub::fold (const juce::String& s)
{
    // Latin letters with accents (Czech, Slovak, German, French, Spanish, Polish...) to their base letter;
    // capitals too (toLowerCase only lowers ASCII).
    static const juce::String from = juce::CharPointer_UTF8 (
        "\xc3\xa0\xc3\xa1\xc3\xa2\xc3\xa3\xc3\xa4\xc3\xa5\xc4\x81\xc4\x83\xc4\x85\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84\xc3\x85\xc4\x80\xc4\x82\xc4\x84"   // àáâãäåāăąÀÁÂÃÄÅĀĂĄ
        "\xc3\xa7\xc4\x87\xc4\x8d\xc3\x87\xc4\x86\xc4\x8c"   // çćčÇĆČ
        "\xc4\x8f\xc4\x91\xc4\x8e\xc4\x90"   // ďđĎĐ
        "\xc3\xa8\xc3\xa9\xc3\xaa\xc3\xab\xc4\x93\xc4\x97\xc4\x99\xc4\x9b\xc3\x88\xc3\x89\xc3\x8a\xc3\x8b\xc4\x92\xc4\x96\xc4\x98\xc4\x9a"   // èéêëēėęěÈÉÊËĒĖĘĚ
        "\xc3\xac\xc3\xad\xc3\xae\xc3\xaf\xc4\xab\xc3\x8c\xc3\x8d\xc3\x8e\xc3\x8f\xc4\xaa"   // ìíîïīÌÍÎÏĪ
        "\xc4\xba\xc4\xbe\xc5\x82\xc4\xb9\xc4\xbd\xc5\x81"   // ĺľłĹĽŁ
        "\xc3\xb1\xc5\x84\xc5\x88\xc3\x91\xc5\x83\xc5\x87"   // ñńňÑŃŇ
        "\xc3\xb2\xc3\xb3\xc3\xb4\xc3\xb5\xc3\xb6\xc3\xb8\xc5\x91\xc3\x92\xc3\x93\xc3\x94\xc3\x95\xc3\x96\xc3\x98\xc5\x90"   // òóôõöøőÒÓÔÕÖØŐ
        "\xc5\x95\xc5\x99\xc5\x94\xc5\x98"   // ŕřŔŘ
        "\xc5\x9b\xc5\xa1\xc5\x9f\xc5\x9a\xc5\xa0\xc5\x9e"   // śšşŚŠŞ
        "\xc5\xa5\xc5\xa4"   // ťŤ
        "\xc3\xb9\xc3\xba\xc3\xbb\xc3\xbc\xc5\xab\xc5\xaf\xc5\xb1\xc3\x99\xc3\x9a\xc3\x9b\xc3\x9c\xc5\xaa\xc5\xae\xc5\xb0"   // ùúûüūůűÙÚÛÜŪŮŰ
        "\xc3\xbd\xc3\xbf\xc3\x9d\xc5\xb8"   // ýÿÝŸ
        "\xc5\xba\xc5\xbc\xc5\xbe\xc5\xb9\xc5\xbb\xc5\xbd");  // źżžŹŻŽ
    static const juce::String to = "aaaaaaaaaaaaaaaaaaccccccddddeeeeeeeeeeeeeeeeiiiiiiiiiillllllnnnnnnoooooooooooooorrrrssssssttuuuuuuuuuuuuuuyyyyzzzzzz";
    jassert (from.length() == to.length());

    auto lower = s.toLowerCase();
    juce::String out;
    out.preallocateBytes ((size_t) lower.length() + 4);
    for (auto c : lower)
    {
        const auto i = from.indexOfChar (c);
        if (i >= 0)
            out << to[i];
        else if (c == 0xdf || c == 0x1e9e)   // ß, ẞ
            out << "ss";
        else
            out << c;
    }
    return out;
}

juce::String PlayRemoteHub::searchJson (RKPlayProcessor& p, const juce::String& query) const
{
    const auto& show = p.getShow();
    const auto words = juce::StringArray::fromTokens (fold (query.substring (0, 80)), " ", {});
    juce::String json;
    int found = 0;
    bool more = false;
    if (! words.isEmpty())
        for (int l = 0; l < (int) show.playlists.size() && ! more; ++l)
        {
            const auto& songs = show.playlists[(size_t) l].songs;
            for (int i = 0; i < (int) songs.size(); ++i)
            {
                const auto& s = songs[(size_t) i];
                const auto text = fold (s.artist + " " + s.title + " " + s.file.getFileNameWithoutExtension());
                // Every word, anywhere: "beatles let" finds "The Beatles - Let It Be".
                bool all = true;
                for (const auto& w : words)
                    if (w.isNotEmpty() && ! text.contains (w)) { all = false; break; }
                if (! all)
                    continue;
                if (found == kMaxResults) { more = true; break; }
                json << (found > 0 ? "," : "") << "[" << l << "," << i << "," << songRow (show, s).substring (1);
                ++found;
            }
        }
    return R"({"type":"search","q":)" + jsonString (query) + ",\"more\":" + bool01 (more) + ",\"results\":[" + json + "]}";
}
