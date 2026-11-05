#include "MixOsc.h"

#include "PluginProcessor.h"
#include "MixScenes.h"

namespace
{
    const juce::String kRoot ("/rkmix/");

    // An argument as text (a name sent as a number is still a name).
    juce::String text (const juce::OSCMessage& m, int index)
    {
        if (m.size() <= index)
            return {};
        const auto& a = m[index];
        return a.isString() ? a.getString() : a.isInt32() ? juce::String (a.getInt32()) : a.isFloat32() ? juce::String (a.getFloat32()) : juce::String();
    }

    // An argument as a whole number (GPScript and TouchOSC may send it as a float or as text); -1: none.
    int whole (const juce::OSCMessage& m, int index)
    {
        if (m.size() <= index)
            return -1;
        const auto& a = m[index];
        if (a.isInt32())   return a.getInt32();
        if (a.isFloat32()) return juce::roundToInt (a.getFloat32());
        if (a.isString())  return a.getString().trim().getIntValue();
        return -1;
    }

    // One line, not too long: a name is shown in a phone's header.
    juce::String clean (const juce::String& s)
    {
        return s.replaceCharacters ("\r\n\t", "   ").trim().substring (0, MixOsc::kMaxLength);
    }
}

MixOsc::MixOsc (RKMixProcessor& p) : mixer (p)
{
    receiver.addListener (this);
}

MixOsc::~MixOsc()
{
    receiver.removeListener (this);
    if (socket != nullptr)
        socket->shutdown();   // the receiver's thread stops waiting on it
    receiver.disconnect();
    socket.reset();
}

//==============================================================================
void MixOsc::begin()
{
    started = true;
    connect();
}

void MixOsc::connect()
{
    if (socket != nullptr)
        socket->shutdown();
    receiver.disconnect();
    socket.reset();
    listening = portTaken = false;
    if (! enabled || ! started)
        return;

    auto s = std::make_unique<juce::DatagramSocket> (false);
    const auto bound = network ? s->bindToPort (port) : s->bindToPort (port, "127.0.0.1");
    if (! bound)
    {
        portTaken = true;
        return;
    }
    socket = std::move (s);
    listening = receiver.connectToSocket (*socket);
    if (! listening)
    {
        socket->shutdown();
        socket.reset();
        portTaken = true;
    }
}

juce::String MixOsc::getStatus() const
{
    if (! enabled)
        return "Off";
    if (portTaken)
        return "Port " + juce::String (port) + " is in use (another RKMix, or another app): choose another port";
    if (! listening)
        return "Not listening";
    return network ? "Listening on port " + juce::String (port) + ", from any computer"
                   : "Listening on 127.0.0.1:" + juce::String (port) + " (this computer)";
}

void MixOsc::setEnabled (bool on)
{
    if (on == enabled)
        return;
    enabled = on;
    if (! on)
        setNow ({});   // no names left over from before
    settingsChanged();
    connect();
}

void MixOsc::setPort (int p)
{
    p = juce::jlimit (1024, 65535, p);
    if (p == port)
        return;
    port = p;
    settingsChanged();
    connect();
}

void MixOsc::setAcceptsFromNetwork (bool on)
{
    if (on == network)
        return;
    network = on;
    settingsChanged();
    connect();
}

juce::var MixOsc::settingsToVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("on", enabled);
    o->setProperty ("port", port);
    o->setProperty ("network", network);
    return juce::var (o);
}

void MixOsc::settingsFromVar (const juce::var& v)
{
    enabled = (bool) v.getProperty ("on", false);
    port = juce::jlimit (1024, 65535, (int) v.getProperty ("port", kDefaultPort));
    network = (bool) v.getProperty ("network", false);
    if (! enabled)
        setNow ({});
    connect();   // only once begun
}

void MixOsc::settingsChanged()
{
    if (onSettingsChanged != nullptr)
        onSettingsChanged();
}

//==============================================================================
MixOsc::Now MixOsc::getNow() const
{
    const juce::ScopedLock sl (nowLock);
    return now;
}

void MixOsc::setNow (const Now& n)
{
    const Now cleaned { clean (n.title), clean (n.subtitle) };
    {
        const juce::ScopedLock sl (nowLock);
        if (cleaned == now)
            return;
        now = cleaned;
    }
    if (onNowChanged != nullptr)
        onNowChanged();
}

juce::String MixOsc::nowJson() const
{
    const auto n = getNow();
    return R"({"t":)" + juce::JSON::toString (juce::var (n.title)) + R"(,"s":)" + juce::JSON::toString (juce::var (n.subtitle)) + "}";
}

//==============================================================================
void MixOsc::handle (const juce::OSCMessage& m)
{
    const auto address = m.getAddressPattern().toString();
    if (! address.startsWith (kRoot))
        return;
    const auto what = address.substring (kRoot.length());

    auto n = getNow();
    if (what == "title")
        n.title = text (m, 0);
    else if (what == "subtitle")
        n.subtitle = text (m, 0);
    else if (what == "now")
        n = { text (m, 0), text (m, 1) };
    else if (what == "clear")
        n = {};
    else if (what == "scene")
    {
        // Scenes 1 to 16, as on the window's buttons.
        if (const auto scene = whole (m, 0); scene >= 1 && scene <= MixScenes::kNumScenes)
            mixer.recallScene (scene - 1);
        return;
    }
    else
        return;
    setNow (n);
}

void MixOsc::oscMessageReceived (const juce::OSCMessage& m)
{
    if (enabled)
        handle (m);
}

void MixOsc::oscBundleReceived (const juce::OSCBundle& bundle)
{
    for (const auto& element : bundle)
    {
        if (element.isMessage())
            oscMessageReceived (element.getMessage());
        else if (element.isBundle())
            oscBundleReceived (element.getBundle());
    }
}
