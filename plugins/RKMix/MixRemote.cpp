#include "MixRemote.h"

#include "PluginProcessor.h"
#include "MixComponents.h"
#include "MixOsc.h"
#include "MixRemotePage.h"
#include "RKMixRemoteFiles.h"

#include <map>
#include <unistd.h>

namespace
{
    constexpr int kSendEqCurvePoints = 61;   // the remote's send EQ curve

    juce::String hexColour (int stripColour)
    {
        return "#" + MixGui::stripColour (stripColour).toDisplayString (false).toLowerCase();
    }

    /** The app icon for a phone's home screen: the console's steel, three faders. */
    juce::MemoryBlock makeIcon()
    {
        constexpr int size = 180;
        juce::Image image (juce::Image::ARGB, size, size, true);
        {
            juce::Graphics g (image);
            g.setColour (juce::Colour (0xff6b8ca6));
            g.fillRect (0, 0, size, size);
            const float x[] = { 52.0f, 90.0f, 128.0f }, cap[] = { 70.0f, 104.0f, 58.0f };
            for (int i = 0; i < 3; ++i)
            {
                g.setColour (juce::Colour (0xff1b1e22).withAlpha (0.55f));
                g.fillRoundedRectangle (x[i] - 4.0f, 32.0f, 8.0f, 116.0f, 4.0f);
                g.setColour (juce::Colours::white);
                g.fillRoundedRectangle (x[i] - 15.0f, cap[i], 30.0f, 18.0f, 5.0f);
            }
        }
        juce::MemoryOutputStream out;
        juce::PNGImageFormat().writeImageToStream (image, out);
        return out.getMemoryBlock();
    }

    juce::String htmlEscape (const juce::String& s)
    {
        return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;").replace ("\"", "&quot;");
    }

    juce::String jsonString (const juce::String& s)
    {
        return juce::JSON::toString (juce::var (s));
    }
}

const char* MixRemotePage::find (const juce::String& name, int& size)
{
   #ifdef RKMIX_REMOTE_FROM_DISK
    // The remote demo tool: the pages as they are on disk (no rebuild for each change).
    {
        static juce::CriticalSection cacheLock;
        static std::map<juce::String, juce::MemoryBlock> cache;
        const juce::ScopedLock sl (cacheLock);
        const auto file = juce::File (RKMIX_REMOTE_FROM_DISK).getChildFile (name);
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
    for (int i = 0; i < RKMixRemoteFiles::namedResourceListSize; ++i)
        if (name == RKMixRemoteFiles::originalFilenames[i])
            return RKMixRemoteFiles::getNamedResource (RKMixRemoteFiles::namedResourceList[i], size);
    size = 0;
    return nullptr;
}

//==============================================================================
// Every 50 ms (sooner when a console has something waiting): each monitor
// page's mix when it changed and its meters every third time; what waits for
// each console, and its meters every time.
class MixRemote::Pusher : public juce::Thread
{
public:
    explicit Pusher (MixRemote& o) : juce::Thread ("RKMix remote push"), owner (o) {}
    ~Pusher() override { stopThread (2000); }

    void run() override
    {
        int meterTicks = 0;
        auto lastMeters = juce::Time::getMillisecondCounter();
        while (! threadShouldExit())
        {
            wait (50);   // notify() wakes it early
            const auto now = juce::Time::getMillisecondCounter();
            juce::String consoleMeters;
            auto sendMeters = false;
            if (now - lastMeters >= 45)
            {
                lastMeters = now;
                MixRemoteMeters m;
                owner.mixer.takeRemoteMeters (m);
                for (size_t i = 0; i < m.channels.size(); ++i)
                    monitorMeters.channels[i] = juce::jmax (monitorMeters.channels[i], m.channels[i]);
                for (size_t i = 0; i < m.buses.size(); ++i)
                    for (size_t side = 0; side < 2; ++side)
                        monitorMeters.buses[i][side] = juce::jmax (monitorMeters.buses[i][side], m.buses[i][side]);
                sendMeters = ++meterTicks % 3 == 0;
                if (owner.consolesOpen.load() > 0)
                    consoleMeters = MixConsole::metersJson (m);
            }
            if (sendMeters)
            {
                channelLevels = monitorMeters.channels;
                busLevels = monitorMeters.buses;
                monitorMeters = {};
            }

            std::map<int, juce::String> mixes, meters;   // by bus, made once for all its phones
            const juce::ScopedLock sl (sentLock);
            owner.server.forEachConnection ([&] (MixWebServer::Connection& c)
            {
                if (c.tag == MixConsole::kTag)
                    return owner.console.send (c, consoleMeters);
                const auto bus = c.tag;
                if (mixes.count (bus) == 0)
                    mixes[bus] = owner.mixFor (bus);
                auto& last = sent[&c];
                if (mixes[bus] != last && c.send (mixes[bus]))
                    last = mixes[bus];
                if (sendMeters)
                {
                    if (meters.count (bus) == 0)
                        meters[bus] = owner.meters (bus);
                    c.send (meters[bus]);
                }
            });
        }
    }

    void forget (MixWebServer::Connection& c)
    {
        const juce::ScopedLock sl (sentLock);
        sent.erase (&c);
    }

    std::array<float, mix::kChannels> channelLevels {};
    std::array<std::array<float, 2>, mix::kBuses> busLevels {};

private:
    MixRemote& owner;
    MixRemoteMeters monitorMeters;
    juce::CriticalSection sentLock;
    std::map<const MixWebServer::Connection*, juce::String> sent;
};

//==============================================================================
MixRemote::MixRemote (RKMixProcessor& p) : mixer (p), console (p), osc (std::make_unique<MixOsc> (p))
{
    osc->onSettingsChanged = [this] { saveSettings(); ++version; };
    osc->onNowChanged = [this]
    {
        ++version;   // the Remote page shows it too
        if (pusher != nullptr)
            pusher->notify();
    };
    console.onSettingsChanged = [this] { saveSettings(); ++version; };
    console.onOutbox = [this]
    {
        if (pusher != nullptr)
            pusher->notify();
    };
    settingsFile = rkfx::PresetLibrary::defaultSettingsFile().getParentDirectory().getChildFile ("RKMix").getChildFile ("remote.json");
    icon = makeIcon();

    server.onHttp = [this] (const juce::String& path) { return http (path); };
    server.onOpen = [this] (MixWebServer::Connection& c)
    {
        if (c.path == "/ws/console")
        {
            if (! console.isEnabled())
                return false;
            c.tag = MixConsole::kTag;
            console.opened (c);
            consolesOpen.fetch_add (1);
            mixer.windowsOpen.fetch_add (1);
            ++version;
            return true;
        }
        const auto bus = busForKey (c.path.fromLastOccurrenceOf ("/", false, false));
        if (! c.path.startsWith ("/ws/") || bus < 0)
            return false;
        if (pusher != nullptr)
            pusher->forget (c);
        c.tag = bus;
        clientsOpen.fetch_add (1);
        mixer.windowsOpen.fetch_add (1);   // the meters measure while a phone shows them
        ++version;
        return true;
    };
    server.onMessage = [this] (MixWebServer::Connection& c, const juce::String& message)
    {
        if (c.tag == MixConsole::kTag)
            console.received (c, message);
        else
            receive (c.tag, message);
    };
    server.onClose = [this] (MixWebServer::Connection& c)
    {
        if (c.tag == MixConsole::kTag)
        {
            console.closed (c);
            consolesOpen.fetch_sub (1);
            mixer.windowsOpen.fetch_sub (1);
        }
        else if (c.tag >= 0)
        {
            clientsOpen.fetch_sub (1);
            mixer.windowsOpen.fetch_sub (1);
        }
        if (pusher != nullptr)
            pusher->forget (c);
        c.tag = -1;
        ++version;
    };

    loadSettings();
}

void MixRemote::begin()
{
    restart();
    osc->begin();
    startTimerHz (30);
}

MixRemote::~MixRemote()
{
    stopTimer();
    pusher.reset();
    server.stop();
    osc.reset();
}

void MixRemote::setSettingsFileForTesting (const juce::File& f)
{
    settingsFile = f;
    loadSettings();
    restart();
}

//==============================================================================
void MixRemote::loadSettings()
{
    juce::var root;
    if (! juce::JSON::parse (settingsFile.loadFileAsString(), root).wasOk())
        root = juce::var();
    enabled = (bool) root.getProperty ("enabled", false);
    port = juce::jlimit (1024, 65535, (int) root.getProperty ("port", 8080));
    useIp = (bool) root.getProperty ("useIp", false);
    secret = root.getProperty ("secret", {}).toString();
    console.settingsFromVar (root.getProperty ("console", {}));
    osc->settingsFromVar (root.getProperty ("osc", {}));
    const auto hadPin = root.getProperty ("console", {}).getProperty ("pin", {}).toString() == console.getPin();
    if (secret.length() < 16)
        secret = juce::Uuid().toString();
    if (! hadPin || secret != root.getProperty ("secret", {}).toString())
        saveSettings();   // a new secret or a new PIN
}

void MixRemote::saveSettings() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("enabled", enabled);
    root->setProperty ("port", port);
    root->setProperty ("useIp", useIp);
    root->setProperty ("secret", secret);
    root->setProperty ("console", console.settingsToVar());
    root->setProperty ("osc", osc->settingsToVar());
    settingsFile.getParentDirectory().createDirectory();
    rkfx::PresetLibrary::writeAtomically (settingsFile, juce::JSON::toString (juce::var (root)));
}

void MixRemote::restart()
{
    pusher.reset();
    server.stop();
    portTaken = false;
    if (enabled)
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

void MixRemote::setEnabled (bool on)
{
    if (on == enabled && on == server.isRunning())
        return;
    enabled = on;
    saveSettings();
    restart();
}

void MixRemote::setPort (int p)
{
    p = juce::jlimit (1024, 65535, p);
    if (p == port)
        return;
    port = p;
    saveSettings();
    restart();
}

void MixRemote::setLinksUseIpAddress (bool on)
{
    useIp = on;
    saveSettings();
    ++version;
}

void MixRemote::newLinks()
{
    secret = juce::Uuid().toString();
    saveSettings();
    restart();   // the phones with the old links are let go
}

juce::String MixRemote::getStatus() const
{
    if (! enabled)
        return "Off";
    if (portTaken)
        return "Port " + juce::String (port) + " is in use (another RKMix, or another app): choose another port";
    return "On at http://" + getHostName() + ":" + juce::String (port);
}

juce::String MixRemote::getHostName() const
{
    if (useIp)
        return juce::IPAddress::getLocalAddress().toString();

    // The computer's Bonjour name ("Richards-MacBook-Pro.local"): it stays when the IP changes.
    char name[256] {};
    if (::gethostname (name, sizeof (name) - 1) != 0 || name[0] == 0)
        return juce::IPAddress::getLocalAddress().toString();
    juce::String host (name);
    if (! host.containsChar ('.'))
        host << ".local";
    return host;
}

juce::String MixRemote::keyFor (int bus) const
{
    const auto text = secret + ":bus:" + juce::String (bus + 1);
    const auto digest = MixWebServer::sha1 (text.toRawUTF8(), text.getNumBytesAsUTF8());
    return juce::String::toHexString (digest.getData(), 6, 0);   // 12 hex digits
}

int MixRemote::busForKey (const juce::String& key) const
{
    for (int bus = 0; bus < mix::kBuses; ++bus)
        if (key == keyFor (bus))
            return bus;
    return -1;
}

juce::String MixRemote::linkFor (int bus) const
{
    return "http://" + getHostName() + ":" + juce::String (port) + "/m/" + keyFor (bus);
}

juce::String MixRemote::consoleLink() const
{
    return "http://" + getHostName() + ":" + juce::String (port) + "/console";
}

std::array<int, mix::kBuses> MixRemote::connectedByBus()
{
    std::array<int, mix::kBuses> n {};
    server.forEachConnection ([&n] (MixWebServer::Connection& c)
    {
        if (c.tag != MixConsole::kTag && juce::isPositiveAndBelow (c.tag, mix::kBuses))
            ++n[(size_t) c.tag];
    });
    return n;
}

//==============================================================================
MixWebServer::Response MixRemote::http (const juce::String& path) const
{
    using Response = MixWebServer::Response;
    if (path == "/icon.png" || path == "/apple-touch-icon.png")
    {
        Response r;
        r.contentType = "image/png";
        r.body = icon;
        return r;
    }

    const auto busName = [this] (int bus)
    {
        const auto name = mixer.getBusName (bus);
        return name.isNotEmpty() ? name : "Bus " + mix::num (bus);
    };

    if (path.startsWith ("/m/"))
    {
        const auto key = path.fromFirstOccurrenceOf ("/m/", false, false).upToFirstOccurrenceOf ("/", false, false);
        const auto bus = busForKey (key);
        if (bus < 0)
            return Response::notFound();
        int size = 0;
        const auto* data = MixRemotePage::find ("monitor.html", size);
        auto page = juce::String::fromUTF8 (data, size);
        page = page.replace ("{{KEY}}", key).replace ("{{TITLE}}", htmlEscape (busName (bus)));
        return Response::text (page);
    }

    // The console app: its page, its scripts, its manifest (the page signs in with the PIN).
    if (path == "/console" || path == "/console/")
    {
        int size = 0;
        const auto* data = MixRemotePage::find ("console.html", size);
        return data != nullptr ? Response::text (juce::String::fromUTF8 (data, size)) : Response::notFound();
    }
    if (path.startsWith ("/c/"))
    {
        const auto name = path.fromFirstOccurrenceOf ("/c/", false, false);
        int size = 0;
        const auto* data = name.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789-_.") ? MixRemotePage::find (name, size) : nullptr;
        if (data == nullptr)
            return Response::notFound();
        Response r;
        r.contentType = name.endsWith (".js") ? "text/javascript; charset=utf-8" : name.endsWith (".css") ? "text/css; charset=utf-8"
                                                                                                           : "text/html; charset=utf-8";
        r.body.append (data, (size_t) size);
        return r;
    }
    if (path == "/manifest/console.json")
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("name", "RKMix Console");
        m->setProperty ("short_name", "RKMix");
        m->setProperty ("start_url", "/console");
        m->setProperty ("scope", "/");
        m->setProperty ("display", "standalone");
        m->setProperty ("orientation", "landscape");
        m->setProperty ("background_color", "#1c1d20");
        m->setProperty ("theme_color", "#1c1d20");
        auto* i = new juce::DynamicObject();
        i->setProperty ("src", "/icon.png");
        i->setProperty ("sizes", "180x180");
        i->setProperty ("type", "image/png");
        m->setProperty ("icons", juce::Array<juce::var> { juce::var (i) });
        return Response::text (juce::JSON::toString (juce::var (m)), "application/manifest+json");
    }

    if (path.startsWith ("/manifest/") && path.endsWith (".json"))
    {
        const auto key = path.fromFirstOccurrenceOf ("/manifest/", false, false).upToLastOccurrenceOf (".json", false, false);
        const auto bus = busForKey (key);
        if (bus < 0)
            return Response::notFound();
        auto* m = new juce::DynamicObject();
        m->setProperty ("name", "RKMix " + busName (bus));
        m->setProperty ("short_name", busName (bus));
        m->setProperty ("start_url", "/m/" + key);
        m->setProperty ("scope", "/m/" + key);
        m->setProperty ("display", "standalone");
        m->setProperty ("background_color", "#1b1e22");
        m->setProperty ("theme_color", "#1b1e22");
        auto* i = new juce::DynamicObject();
        i->setProperty ("src", "/icon.png");
        i->setProperty ("sizes", "180x180");
        i->setProperty ("type", "image/png");
        m->setProperty ("icons", juce::Array<juce::var> { juce::var (i) });
        return Response::text (juce::JSON::toString (juce::var (m)), "application/manifest+json");
    }

    if (path == "/")
        return Response::text ("<!doctype html><meta name=viewport content=\"width=device-width\"><body style=\"font:16px system-ui;"
                               "background:#1b1e22;color:#e8eaed;padding:24px\"><h2>RKMix</h2><p>Scan your mix's QR code on RKMix's "
                               "Remote page to open it here.</p>");
    return Response::notFound();
}

juce::String MixRemote::mixFor (int bus) const
{
    if (! juce::isPositiveAndBelow (bus, mix::kBuses))
        return {};
    if (mixer.getBusType (bus) != mix::BusType::aux)
        return R"({"type":"gone","text":"This bus is no longer a monitor mix"})";

    auto& s = mixer.getSettings();
    const auto own = mixer.isBusStereo (bus) && s.choice (mix::busId (bus, "panmode")) == (int) mix::PanMode::own;
    const auto busName = mixer.getBusName (bus);

    juce::String json;
    json << R"({"type":"mix","bus":{"n":)" << (bus + 1)
         << R"(,"name":)" << jsonString (busName.isNotEmpty() ? busName : "Bus " + mix::num (bus))
         << R"(,"colour":")" << hexColour (mixer.getBusColour (bus))
         << R"(","stereo":)" << (mixer.isBusStereo (bus) ? "true" : "false")
         << R"(,"own":)" << (own ? "true" : "false")
         << R"(,"fader":)" << juce::String (rkfx::ProcessorBase::valueOf (mixer.findParam (mix::busFaderId (bus))), 2)
         << R"(,"mute":)" << (mixer.findParam (mix::busMuteId (bus))->getValue() >= 0.5f ? "true" : "false")
         << R"(},"now":)" << osc->nowJson()
         << R"(,"rows":[)";

    bool first = true;
    const auto row = [&] (const juce::String& key, const juce::String& name, int colour, float sendDb, float pan, bool used, bool isReturn,
                          const juce::String& eq = {})
    {
        json << (first ? "" : ",") << R"({"k":")" << key << R"(","name":)" << jsonString (name)
             << R"(,"colour":")" << hexColour (colour) << R"(","send":)" << juce::String (sendDb, 2)
             << R"(,"pan":)" << juce::String (pan, 1) << R"(,"used":)" << (used ? "true" : "false")
             << R"(,"ret":)" << (isReturn ? "true" : "false") << eq << "}";
        first = false;
    };

    // A channel's send EQ to this bus: its settings (normalised, in mix::sendEqFields order) and,
    // when it isn't flat, its curve (dB at 61 frequencies, 20 Hz to 20 kHz, evenly on a log scale).
    const auto rate = mixer.getSampleRate() > 0.0 ? mixer.getSampleRate() : 48000.0;
    const auto sendEq = [&] (int ch)
    {
        juce::String e (",\"eq\":[");
        const auto& params = mixer.getSendEqParams (ch, bus);
        for (int f = 0; f < mix::kSendEqFields; ++f)
            e << (f > 0 ? "," : "") << juce::String (params[(size_t) f]->getValue(), 4);
        e << "]";
        if (const auto settings = mixer.getSendEq (ch, bus); ! settings.isFlat())
        {
            e << ",\"eqc\":[";
            for (int i = 0; i < kSendEqCurvePoints; ++i)
            {
                const auto hz = 20.0 * std::pow (1000.0, (double) i / (kSendEqCurvePoints - 1));
                e << (i > 0 ? "," : "") << juce::String (SendEqDsp::responseDb (settings, hz, rate), 1);
            }
            e << "]";
        }
        return e;
    };

    for (int ch = 0; ch < mix::kChannels; ++ch)
    {
        if (ch % 2 == 1 && mixer.isLinked (ch / 2))
            continue;
        const auto name = mixer.getChannelName (ch);
        const auto label = mixer.isLinked (ch / 2) && ch % 2 == 0 ? mix::num (ch) + "/" + mix::num (ch + 1) : mix::num (ch);
        const auto sendDb = s.value (mix::channelSendId (ch, bus));
        row ("ch" + mix::num (ch), name.isNotEmpty() ? name : "Ch " + label, mixer.getChannelColour (ch), sendDb,
             s.value (mix::channelSendPanId (ch, bus)), name.isNotEmpty() || sendDb > mix::kFaderMinDb + 0.001f, false, sendEq (ch));
    }
    // The returns that can feed it: FX (a reverb in the in-ear) and Group buses.
    for (int from = 0; from < mix::kBuses; ++from)
        if (mixer.canSend (from, bus))
        {
            const auto name = mixer.getBusName (from);
            const auto sendDb = s.value (mix::busSendId (from, bus));
            row ("bus" + mix::num (from), name.isNotEmpty() ? name : "Bus " + mix::num (from), mixer.getBusColour (from), sendDb, 0.0f,
                 name.isNotEmpty() || sendDb > mix::kFaderMinDb + 0.001f, true);
        }
    json << "]}";
    return json;
}

juce::String MixRemote::meters (int bus) const
{
    if (pusher == nullptr)
        return {};
    const auto& ch = pusher->channelLevels;
    const auto& b = pusher->busLevels;
    juce::String json;
    json << R"({"type":"meters","bus":)" << juce::String (juce::jmax (b[(size_t) bus][0], b[(size_t) bus][1]), 4) << R"(,"rows":{)";
    bool first = true;
    for (int c = 0; c < mix::kChannels; ++c)
    {
        if (c % 2 == 1 && mixer.isLinked (c / 2))
            continue;
        auto level = ch[(size_t) c];
        if (c % 2 == 0 && mixer.isLinked (c / 2))
            level = juce::jmax (level, ch[(size_t) c + 1]);
        if (level < 0.0001f)
            continue;
        json << (first ? "" : ",") << "\"ch" << (c + 1) << "\":" << juce::String (level, 4);
        first = false;
    }
    for (int from = 0; from < mix::kBuses; ++from)
        if (const auto level = juce::jmax (b[(size_t) from][0], b[(size_t) from][1]); level >= 0.0001f && mixer.canSend (from, bus))
        {
            json << (first ? "" : ",") << "\"bus" << (from + 1) << "\":" << juce::String (level, 4);
            first = false;
        }
    json << "}}";
    return json;
}

//==============================================================================
void MixRemote::receive (int bus, const juce::String& message, bool now)
{
    if (! juce::isPositiveAndBelow (bus, mix::kBuses) || mixer.getBusType (bus) != mix::BusType::aux)
        return;
    const auto msg = juce::JSON::parse (message);
    const auto type = msg["t"].toString();
    const auto value = (float) (double) msg["v"];
    const auto key = msg["k"].toString();
    auto& s = mixer.getSettings();

    // Only this bus's own controls: never anything else of the mix.
    juce::RangedAudioParameter* param = nullptr;
    auto realValue = value;
    if (type == "fader")
        param = mixer.findParam (mix::busFaderId (bus));
    else if (type == "mute")
        param = mixer.findParam (mix::busMuteId (bus));
    else if (type == "send" || type == "pan")
    {
        const auto n = key.retainCharacters ("0123456789").getIntValue() - 1;
        if (key.startsWith ("ch") && juce::isPositiveAndBelow (n, mix::kChannels) && (n % 2 == 0 || ! mixer.isLinked (n / 2)))
            param = &s.param (type == "send" ? mix::channelSendId (n, bus) : mix::channelSendPanId (n, bus));
        else if (key.startsWith ("bus") && type == "send" && juce::isPositiveAndBelow (n, mix::kBuses) && mixer.canSend (n, bus))
            param = &s.param (mix::busSendId (n, bus));
    }
    else if (type == "seq")
    {
        // A channel's send EQ to this bus: one field, normalised ("n").
        const auto n = key.retainCharacters ("0123456789").getIntValue() - 1;
        const auto field = msg["f"].toString();
        const auto normalised = (float) (double) msg["n"];
        if (key.startsWith ("ch") && juce::isPositiveAndBelow (n, mix::kChannels) && (n % 2 == 0 || ! mixer.isLinked (n / 2))
            && std::isfinite (normalised))
            for (int f = 0; f < mix::kSendEqFields; ++f)
                if (field == mix::sendEqFields[f])
                {
                    param = mixer.getSendEqParams (n, bus)[(size_t) f];
                    realValue = param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised));
                }
    }
    if (param == nullptr || ! std::isfinite (realValue))
        return;

    {
        const juce::ScopedLock sl (pendingLock);
        for (auto& p : pending)
            if (p.first == param)
            {
                p.second = realValue;   // only the latest of a drag
                param = nullptr;
                break;
            }
        if (param != nullptr)
            pending.emplace_back (param, realValue);
    }
    if (now)
        applyPending();
}

void MixRemote::timerCallback()
{
    applyPending();
    console.tick();
}

void MixRemote::applyPending()
{
    std::vector<std::pair<juce::RangedAudioParameter*, float>> changes;
    {
        const juce::ScopedLock sl (pendingLock);
        changes.swap (pending);
    }
    for (auto& [param, value] : changes)
    {
        const auto n = param->convertTo0to1 (param->getNormalisableRange().snapToLegalValue (value));
        if (! juce::exactlyEqual (param->getValue(), n))
            param->setValueNotifyingHost (n);
    }
}
