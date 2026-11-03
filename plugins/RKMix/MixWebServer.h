#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>
#include <vector>

/** A small HTTP and WebSocket server for the local network: plain HTTP
    GETs answered by a callback, and WebSocket connections (RFC 6455, text
    messages) with a callback per message. One thread listens, one thread
    per connection reads; sending is thread-safe. No TLS, no keep-alive. */
class MixWebServer
{
public:
    struct Response
    {
        int status = 200;
        juce::String contentType = "text/html; charset=utf-8";
        juce::MemoryBlock body;
        static Response text (const juce::String& s, const juce::String& type = "text/html; charset=utf-8");
        static Response notFound();
    };

    class Connection
    {
    public:
        virtual ~Connection() = default;
        /** Sends a text message (any thread). False once the connection is gone. */
        virtual bool send (const juce::String& text) = 0;
        virtual void close() = 0;
        juce::String path;              // what the client asked for: "/ws/3f9a2c71b0"
        juce::String address;           // the client's IP
        int tag = -1;                   // free for the owner (RKMix: the bus)
    };

    /** GET @p path (any thread): the answer. */
    std::function<Response (const juce::String& path)> onHttp;
    /** A WebSocket was opened at @p path; return false to refuse it (any thread). */
    std::function<bool (Connection&)> onOpen;
    std::function<void (Connection&, const juce::String& message)> onMessage;
    std::function<void (Connection&)> onClose;

    MixWebServer();
    ~MixWebServer();

    /** Listens on @p port (all interfaces). False when the port is taken. */
    bool start (int port);
    void stop();
    bool isRunning() const noexcept;
    int getPort() const noexcept { return port; }

    /** Calls @p f with each open WebSocket (under a lock: keep it short). */
    void forEachConnection (const std::function<void (Connection&)>& f);
    int numConnections();

    /** SHA-1 of @p data (the WebSocket handshake; RKMix's remote links). */
    static juce::MemoryBlock sha1 (const void* data, size_t size);

private:
    class Listener;
    class Client;
    std::unique_ptr<Listener> listener;
    juce::CriticalSection clientsLock;
    std::vector<std::unique_ptr<Client>> clients;
    int port = 0;

    void serve (std::unique_ptr<juce::StreamingSocket>);
    void removeFinished();
    friend class Listener;
    friend class Client;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixWebServer)
};
