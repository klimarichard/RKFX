#include "RemoteServer.h"

#include <array>

namespace rkfx
{

//==============================================================================
juce::MemoryBlock RemoteServer::sha1 (const void* data, size_t size)
{
    std::array<std::uint32_t, 5> h { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
    const auto rol = [] (std::uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };

    // The message, a 1 bit, zeros, and its length in bits: a whole number of 64-byte chunks.
    std::vector<std::uint8_t> m ((const std::uint8_t*) data, (const std::uint8_t*) data + size);
    const auto bits = (std::uint64_t) size * 8;
    m.push_back (0x80);
    while (m.size() % 64 != 56)
        m.push_back (0);
    for (int i = 7; i >= 0; --i)
        m.push_back ((std::uint8_t) (bits >> (i * 8)));

    for (size_t chunk = 0; chunk < m.size(); chunk += 64)
    {
        std::array<std::uint32_t, 80> w {};
        for (int i = 0; i < 16; ++i)
            w[(size_t) i] = (std::uint32_t) m[chunk + (size_t) i * 4] << 24 | (std::uint32_t) m[chunk + (size_t) i * 4 + 1] << 16
                          | (std::uint32_t) m[chunk + (size_t) i * 4 + 2] << 8 | (std::uint32_t) m[chunk + (size_t) i * 4 + 3];
        for (size_t i = 16; i < 80; ++i)
            w[i] = rol (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        auto a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (size_t i = 0; i < 80; ++i)
        {
            std::uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
            const auto t = rol (a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol (b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }

    juce::MemoryBlock out (20);
    for (int i = 0; i < 20; ++i)
        static_cast<std::uint8_t*> (out.getData())[i] = (std::uint8_t) (h[(size_t) i / 4] >> (24 - (i % 4) * 8));
    return out;
}

RemoteServer::Response RemoteServer::Response::text (const juce::String& s, const juce::String& type)
{
    Response r;
    r.contentType = type;
    r.body.append (s.toRawUTF8(), s.getNumBytesAsUTF8());
    return r;
}

RemoteServer::Response RemoteServer::Response::notFound()
{
    auto r = text ("Not found", "text/plain; charset=utf-8");
    r.status = 404;
    return r;
}

//==============================================================================
class RemoteServer::Listener : public juce::Thread
{
public:
    Listener (RemoteServer& o) : juce::Thread ("RKFX remote"), owner (o) {}
    ~Listener() override
    {
        socket.close();
        stopThread (2000);
    }

    bool open (int portNumber) { return socket.createListener (portNumber); }

    void run() override
    {
        while (! threadShouldExit())
        {
            std::unique_ptr<juce::StreamingSocket> client (socket.waitForNextConnection());
            if (threadShouldExit())
                break;
            if (client != nullptr)
                owner.serve (std::move (client));
            owner.removeFinished();
        }
    }

    juce::StreamingSocket socket;
    RemoteServer& owner;
};

//==============================================================================
class RemoteServer::Client : public juce::Thread,
                             public RemoteServer::Connection
{
public:
    Client (RemoteServer& o, std::unique_ptr<juce::StreamingSocket> s)
        : juce::Thread ("RKMix remote client"), owner (o), socket (std::move (s))
    {
        address = socket->getHostName();
    }

    ~Client() override
    {
        close();
        stopThread (2000);
    }

    bool send (const juce::String& text) override { return sendFrame (0x1, text.toRawUTF8(), text.getNumBytesAsUTF8()); }

    void close() override
    {
        const juce::ScopedLock sl (writeLock);
        open = false;
        socket->close();
    }

    bool finished() const noexcept { return done.load(); }

    void run() override
    {
        handle();
        if (isWebSocket && owner.onClose != nullptr)
            owner.onClose (*this);
        close();
        done = true;
    }

private:
    RemoteServer& owner;
    std::unique_ptr<juce::StreamingSocket> socket;
    juce::CriticalSection writeLock;
    bool open = true, isWebSocket = false;
    std::atomic<bool> done { false };

    /** Reads exactly @p size bytes, waiting up to @p timeoutMs each time (or until told to stop). */
    bool readExactly (void* dest, int size, int timeoutMs = -1)
    {
        auto* p = static_cast<char*> (dest);
        int got = 0;
        const auto start = juce::Time::getMillisecondCounter();
        while (got < size)
        {
            if (threadShouldExit() || ! socket->isConnected())
                return false;
            const auto ready = socket->waitUntilReady (true, 100);
            if (ready < 0)
                return false;
            if (ready == 0)
            {
                if (timeoutMs >= 0 && (int) (juce::Time::getMillisecondCounter() - start) > timeoutMs)
                    return false;
                continue;
            }
            const auto n = socket->read (p + got, size - got, false);
            if (n <= 0)
                return false;
            got += n;
        }
        return true;
    }

    bool writeAll (const void* data, size_t size)
    {
        const juce::ScopedLock sl (writeLock);
        if (! open)
            return false;
        return socket->write (data, (int) size) == (int) size;
    }

    bool sendFrame (int opcode, const void* data, size_t size)
    {
        juce::MemoryOutputStream frame;
        frame.writeByte ((char) (0x80 | opcode));
        if (size < 126)
            frame.writeByte ((char) size);
        else if (size < 65536)
        {
            frame.writeByte ((char) 126);
            frame.writeShortBigEndian ((short) size);
        }
        else
        {
            frame.writeByte ((char) 127);
            frame.writeInt64BigEndian ((juce::int64) size);
        }
        frame.write (data, size);
        return writeAll (frame.getData(), frame.getDataSize());
    }

    void respond (const Response& r)
    {
        const auto statusText = r.status == 200 ? "OK" : r.status == 404 ? "Not Found" : "Error";
        const auto head = "HTTP/1.1 " + juce::String (r.status) + " " + statusText + "\r\n"
                        + "Content-Type: " + r.contentType + "\r\n"
                        + "Content-Length: " + juce::String ((int) r.body.getSize()) + "\r\n"
                        + "Cache-Control: no-cache\r\nConnection: close\r\n\r\n";
        writeAll (head.toRawUTF8(), head.getNumBytesAsUTF8());
        writeAll (r.body.getData(), r.body.getSize());
    }

    void handle()
    {
        // The request's head, up to the blank line (8 kB at most, 5 s).
        juce::MemoryOutputStream head;
        for (char c = 0; head.getDataSize() < 8192;)
        {
            if (! readExactly (&c, 1, 5000))
                return;
            head.writeByte (c);
            if (head.getDataSize() >= 4 && juce::String (static_cast<const char*> (head.getData()) + head.getDataSize() - 4, 4) == "\r\n\r\n")
                break;
        }
        const auto lines = juce::StringArray::fromLines (head.toString());
        const auto request = juce::StringArray::fromTokens (lines[0], " ", {});
        if (request.size() < 2 || request[0] != "GET")
            return respond (Response::notFound());
        path = juce::URL::removeEscapeChars (request[1].upToFirstOccurrenceOf ("?", false, false));

        juce::String key;
        bool upgrade = false;
        for (const auto& line : lines)
        {
            const auto name = line.upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
            const auto value = line.fromFirstOccurrenceOf (":", false, false).trim();
            if (name == "sec-websocket-key")
                key = value;
            if (name == "upgrade" && value.equalsIgnoreCase ("websocket"))
                upgrade = true;
        }

        if (! upgrade || key.isEmpty())
            return respond (owner.onHttp != nullptr ? owner.onHttp (path) : Response::notFound());

        // The WebSocket handshake.
        if (owner.onOpen == nullptr || ! owner.onOpen (*this))
            return respond (Response::notFound());
        const auto magic = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        const auto digest = sha1 (magic.toRawUTF8(), magic.getNumBytesAsUTF8());
        const auto accept = juce::Base64::toBase64 (digest.getData(), digest.getSize());
        const auto reply = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
        isWebSocket = true;
        if (! writeAll (reply.toRawUTF8(), reply.getNumBytesAsUTF8()))
            return;
        readMessages();
    }

    void readMessages()
    {
        juce::MemoryBlock message;
        while (! threadShouldExit())
        {
            std::uint8_t h[2];
            if (! readExactly (h, 2))
                return;
            const auto fin = (h[0] & 0x80) != 0;
            const auto opcode = h[0] & 0x0F;
            const auto masked = (h[1] & 0x80) != 0;
            std::uint64_t length = h[1] & 0x7F;
            if (length == 126)
            {
                std::uint8_t l[2];
                if (! readExactly (l, 2)) return;
                length = (std::uint64_t) l[0] << 8 | l[1];
            }
            else if (length == 127)
            {
                std::uint8_t l[8];
                if (! readExactly (l, 8)) return;
                length = 0;
                for (auto b : l)
                    length = length << 8 | b;
            }
            if (length > 1 << 20)
                return;   // a remote sends short messages
            std::uint8_t mask[4] {};
            if (masked && ! readExactly (mask, 4))
                return;
            juce::MemoryBlock payload ((size_t) length);
            if (length > 0 && ! readExactly (payload.getData(), (int) length))
                return;
            auto* bytes = static_cast<std::uint8_t*> (payload.getData());
            for (size_t i = 0; i < (size_t) length; ++i)
                bytes[i] ^= mask[i % 4];

            switch (opcode)
            {
                case 0x0:   // a continuation
                case 0x1:   // text
                    message.append (payload.getData(), payload.getSize());
                    if (fin)
                    {
                        if (owner.onMessage != nullptr)
                            owner.onMessage (*this, message.toString());
                        message.reset();
                    }
                    break;
                case 0x8:   // close
                    sendFrame (0x8, nullptr, 0);
                    return;
                case 0x9:   // ping
                    sendFrame (0xA, payload.getData(), payload.getSize());
                    break;
                default:
                    break;
            }
        }
    }
};

//==============================================================================
RemoteServer::RemoteServer() = default;

RemoteServer::~RemoteServer()
{
    stop();
}

bool RemoteServer::start (int p)
{
    stop();
    auto l = std::make_unique<Listener> (*this);
    if (! l->open (p))
        return false;
    port = p;
    listener = std::move (l);
    listener->startThread();
    return true;
}

void RemoteServer::stop()
{
    listener.reset();
    std::vector<std::unique_ptr<Client>> old;
    {
        const juce::ScopedLock sl (clientsLock);
        old.swap (clients);
    }
    old.clear();   // each closes its socket and stops its thread
    port = 0;
}

bool RemoteServer::isRunning() const noexcept
{
    return listener != nullptr;
}

void RemoteServer::serve (std::unique_ptr<juce::StreamingSocket> socket)
{
    auto client = std::make_unique<Client> (*this, std::move (socket));
    client->startThread();
    const juce::ScopedLock sl (clientsLock);
    clients.push_back (std::move (client));
}

void RemoteServer::removeFinished()
{
    std::vector<std::unique_ptr<Client>> finished;
    {
        const juce::ScopedLock sl (clientsLock);
        for (auto it = clients.begin(); it != clients.end();)
            if ((*it)->finished())
            {
                finished.push_back (std::move (*it));
                it = clients.erase (it);
            }
            else
                ++it;
    }
}

void RemoteServer::forEachConnection (const std::function<void (Connection&)>& f)
{
    const juce::ScopedLock sl (clientsLock);
    for (auto& c : clients)
        if (! c->finished() && c->tag >= 0)
            f (*c);
}

int RemoteServer::numConnections()
{
    int n = 0;
    forEachConnection ([&n] (Connection&) { ++n; });
    return n;
}

} // namespace rkfx
