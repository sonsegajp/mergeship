// mergeship.exe's one Anchor connection (NetService.h, include/mergeship/AnchorNet.h).
//
// A thread owns the socket: it connects (and reconnects, with back-off, for as long as a connection is wanted), sends
// the stored hello first on every connect, flushes what the engines queued with Send, and splits what arrives at the
// NUL delimiters into packets, each queued for every engine with a process-wide sequence number. The engines' own
// Anchor threads drain their queues whether their engine is running or suspended, so nothing about a world switch
// touches the connection. TCP keep-alive is on (30 s idle, 5 s probes) so an idle link through a NAT survives a long
// stay in one world.
#include "NetService.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#define MS_WOULD_BLOCK() (WSAGetLastError() == WSAEWOULDBLOCK)
#define MS_SEND_FLAGS 0
#else
// POSIX (Android): the same calls under their BSD names.
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int SOCKET;
typedef int BOOL;
#define TRUE 1
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define MS_WOULD_BLOCK() ((errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == EINPROGRESS))
#define MS_SEND_FLAGS MSG_NOSIGNAL // a closed peer is an error, not SIGPIPE
#endif

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace MergeShipHost {

namespace {

// One connection per process: the MsHostNet trampolines ignore the host pointer the engines pass back.
NetService* sInstance = nullptr;

constexpr size_t kMaxQueuedPerEngine = 20000;
constexpr size_t kMaxPacketBytes = 16 * 1024 * 1024; // a runaway stream without delimiters is dropped
constexpr size_t kMaxClaims = 200000;

int32_t ApiConnect(void*, const char* address, uint16_t port, const char* hello, uint32_t helloLength) {
    return sInstance != nullptr ? sInstance->Connect(address, port, hello, helloLength) : 0;
}
void ApiDisconnect(void*) {
    if (sInstance != nullptr) {
        sInstance->Disconnect();
    }
}
void ApiSetHello(void*, const char* hello, uint32_t length) {
    if (sInstance != nullptr) {
        sInstance->SetHello(hello, length);
    }
}
int32_t ApiSend(void*, const char* data, uint32_t length) {
    return sInstance != nullptr ? sInstance->Send(data, length) : 0;
}
int32_t ApiReceive(void*, int32_t engineId, char* buffer, uint32_t capacity, uint64_t* seq, uint32_t* connectionId) {
    return sInstance != nullptr ? sInstance->Receive(engineId, buffer, capacity, seq, connectionId) : 0;
}
void ApiGetStatus(void*, MsNetStatus* status) {
    if (status == nullptr) {
        return;
    }
    const uint32_t callerSize = status->size;
    MsNetStatus current = {};
    if (sInstance != nullptr) {
        current = sInstance->Status();
    }
    current.size = sizeof(MsNetStatus);
    memcpy(status, &current, std::min<size_t>(callerSize != 0 ? callerSize : sizeof(MsNetStatus), sizeof(current)));
    status->size = callerSize;
}
int32_t ApiClaim(void*, uint64_t seq, int32_t engineId) {
    return sInstance != nullptr ? sInstance->Claim(seq, engineId) : 1;
}
void ApiSetBlob(void*, const char* key, const char* data, uint32_t length) {
    if (sInstance != nullptr) {
        sInstance->SetBlob(key, data, length);
    }
}
int32_t ApiGetBlob(void*, const char* key, char* buffer, uint32_t capacity) {
    return sInstance != nullptr ? sInstance->GetBlob(key, buffer, capacity) : 0;
}

bool SendAll(SOCKET socket, const char* data, size_t length) {
    while (length > 0) {
        const int sent = send(socket, data, (int)std::min<size_t>(length, 1 << 20), MS_SEND_FLAGS);
        if (sent == SOCKET_ERROR) {
            if (MS_WOULD_BLOCK()) {
                fd_set writable;
                FD_ZERO(&writable);
                FD_SET(socket, &writable);
                timeval timeout = { 1, 0 };
                if (select((int)socket + 1, nullptr, &writable, nullptr, &timeout) <= 0) {
                    return false;
                }
                continue;
            }
            return false;
        }
        data += sent;
        length -= (size_t)sent;
    }
    return true;
}

} // namespace

NetService::NetService() : mSocket((uintptr_t)INVALID_SOCKET) {
    mApi.size = sizeof(MsHostNet);
    mApi.Connect = ApiConnect;
    mApi.Disconnect = ApiDisconnect;
    mApi.SetHello = ApiSetHello;
    mApi.Send = ApiSend;
    mApi.Receive = ApiReceive;
    mApi.GetStatus = ApiGetStatus;
    mApi.Claim = ApiClaim;
    mApi.SetBlob = ApiSetBlob;
    mApi.GetBlob = ApiGetBlob;
}

NetService::~NetService() {
    Shutdown();
}

void NetService::Init(const std::vector<int32_t>& engines, LogFn log) {
    mLog = std::move(log);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (int32_t engine : engines) {
            mInbox[engine];
        }
    }
#ifdef _WIN32
    WSADATA wsa;
    mWsaStarted = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    if (!mWsaStarted) {
        Logf("net: WSAStartup failed (%d); Anchor cannot connect", WSAGetLastError());
    }
#else
    mWsaStarted = true;
#endif
    sInstance = this;
    mThread = std::thread(&NetService::Run, this);
}

void NetService::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mQuit) {
            return;
        }
        mQuit = true;
    }
    mWake.notify_all();
    if (mThread.joinable()) {
        mThread.join();
    }
    if (sInstance == this) {
        sInstance = nullptr;
    }
    if (mWsaStarted) {
#ifdef _WIN32
        WSACleanup();
#endif
        mWsaStarted = false;
    }
}

void NetService::Logf(const char* format, ...) {
    if (!mLog) {
        return;
    }
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    mLog(buffer);
}

void NetService::SetActiveEngine(int32_t engineId) {
    uint32_t connection = mConnectionId.load();
    bool wanted;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mActiveEngine == engineId) {
            return;
        }
        mActiveEngine = engineId;
        wanted = mWanted;
    }
    if (wanted) {
        // The line tools/test_anchor.ps1 checks: one connection, whichever world the player is in.
        Logf("net: active engine %d, Anchor connection %u %s (reconnects %u)", engineId, connection,
             mConnected.load() ? "up" : "down", mReconnects.load());
    }
}

MsNetStatus NetService::Status() {
    MsNetStatus status = {};
    status.size = sizeof(MsNetStatus);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        status.enabled = mWanted ? 1 : 0;
        status.activeEngine = mActiveEngine;
        snprintf(status.address, sizeof(status.address), "%s", mAddress.c_str());
        status.port = mPort;
    }
    status.connected = (status.enabled && mConnected.load()) ? 1 : 0;
    status.connectionId = mConnectionId.load();
    status.reconnects = mReconnects.load();
    status.packetsIn = mPacketsIn.load();
    status.packetsOut = mPacketsOut.load();
    return status;
}

int32_t NetService::Connect(const char* address, uint16_t port, const char* hello, uint32_t helloLength) {
    if (address == nullptr) {
        return 0;
    }
    int32_t result = 1;
    bool started = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (hello != nullptr && helloLength > 0) {
            mHello.assign(hello, helloLength);
        }
        if (mWanted) {
            // Already connected (or connecting) for the other engine: same room, or the other engine's stays.
            result = (mAddress == address && mPort == port) ? 1 : 0;
        } else {
            mWanted = true;
            mWantGeneration++;
            mAddress = address;
            mPort = port;
            mOutbox.clear();
            started = true;
        }
    }
    if (started) {
        Logf("net: Anchor connection wanted: %s:%u", address, port);
        mWake.notify_all();
    }
    return result;
}

void NetService::Disconnect() {
    bool was;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        was = mWanted;
        mWanted = false;
        mWantGeneration++;
        mOutbox.clear();
        for (auto& [engine, queue] : mInbox) {
            queue.clear();
        }
    }
    if (was) {
        Logf("net: Anchor disabled: leaving the room (connection %u)", mConnectionId.load());
    }
    mWake.notify_all();
}

void NetService::SetHello(const char* hello, uint32_t length) {
    if (hello == nullptr || length == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    mHello.assign(hello, length);
}

int32_t NetService::Send(const char* data, uint32_t length) {
    if (data == nullptr || length == 0 || !mConnected.load()) {
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mWanted) {
            return 0;
        }
        mOutbox.emplace_back(data, length);
    }
    mWake.notify_all();
    return 1;
}

int32_t NetService::Receive(int32_t engineId, char* buffer, uint32_t capacity, uint64_t* seq, uint32_t* connectionId) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mInbox.find(engineId);
    if (it == mInbox.end() || it->second.empty()) {
        return 0;
    }
    Packet& packet = it->second.front();
    const size_t needed = packet.data.size() + 1;
    if (buffer == nullptr || capacity < needed) {
        return -(int32_t)needed;
    }
    memcpy(buffer, packet.data.data(), packet.data.size());
    buffer[packet.data.size()] = '\0';
    if (seq != nullptr) {
        *seq = packet.seq;
    }
    if (connectionId != nullptr) {
        *connectionId = packet.connectionId;
    }
    const int32_t length = (int32_t)packet.data.size();
    it->second.pop_front();
    return length;
}

int32_t NetService::Claim(uint64_t seq, int32_t engineId) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mClaims.find(seq) != mClaims.end()) {
        return 0;
    }
    if (engineId < 0) {
        return 1; // only asking
    }
    if (mClaims.size() >= kMaxClaims) {
        // Nothing waits that long in a queue (kMaxQueuedPerEngine): forget the oldest half.
        const uint64_t keepFrom = mNextSeq > kMaxClaims / 2 ? mNextSeq - kMaxClaims / 2 : 0;
        for (auto it = mClaims.begin(); it != mClaims.end();) {
            it = (it->first < keepFrom) ? mClaims.erase(it) : std::next(it);
        }
    }
    mClaims.emplace(seq, engineId);
    return 1;
}

void NetService::SetBlob(const char* key, const char* data, uint32_t length) {
    if (key == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    if (data == nullptr || length == 0) {
        mBlobs.erase(key);
    } else {
        mBlobs[key].assign(data, length);
    }
}

int32_t NetService::GetBlob(const char* key, char* buffer, uint32_t capacity) {
    if (key == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mBlobs.find(key);
    if (it == mBlobs.end()) {
        return 0;
    }
    const size_t needed = it->second.size() + 1;
    if (buffer == nullptr || capacity < needed) {
        return -(int32_t)needed;
    }
    memcpy(buffer, it->second.data(), it->second.size());
    buffer[it->second.size()] = '\0';
    return (int32_t)it->second.size();
}

// ---- the network thread --------------------------------------------------------------------------------------------

bool NetService::OpenSocket(const std::string& address, uint16_t port) {
    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* found = nullptr;
    const std::string service = std::to_string(port);
    if (getaddrinfo(address.c_str(), service.c_str(), &hints, &found) != 0 || found == nullptr) {
        return false;
    }

    SOCKET result = INVALID_SOCKET;
    for (addrinfo* ai = found; ai != nullptr && result == INVALID_SOCKET; ai = ai->ai_next) {
        SOCKET s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == INVALID_SOCKET) {
            continue;
        }
        // Non-blocking connect with a timeout, so a Disconnect is not stuck behind an unreachable server.
#ifdef _WIN32
        u_long nonBlocking = 1;
        ioctlsocket(s, FIONBIO, &nonBlocking);
#else
        fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
        if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) == SOCKET_ERROR && !MS_WOULD_BLOCK()) {
            closesocket(s);
            continue;
        }
        fd_set writable, failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(s, &writable);
        FD_SET(s, &failed);
        timeval timeout = { 5, 0 };
        if (select((int)s + 1, nullptr, &writable, &failed, &timeout) <= 0 || FD_ISSET(s, &failed)) {
            closesocket(s);
            continue;
        }
        result = s;
    }
    freeaddrinfo(found);
    if (result == INVALID_SOCKET) {
        return false;
    }

    BOOL on = TRUE;
    setsockopt(result, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(on));
    setsockopt(result, SOL_SOCKET, SO_KEEPALIVE, (const char*)&on, sizeof(on));
#ifdef _WIN32
    tcp_keepalive keepAlive = { 1, 30000, 5000 };
    DWORD returned = 0;
    WSAIoctl(result, SIO_KEEPALIVE_VALS, &keepAlive, sizeof(keepAlive), nullptr, 0, &returned, nullptr, nullptr);
#else
    int idle = 30;
    int interval = 5;
    setsockopt(result, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(result, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
#endif
    mSocket = (uintptr_t)result;
    return true;
}

void NetService::CloseSocket(const char* why) {
    if ((SOCKET)mSocket == INVALID_SOCKET) {
        return;
    }
    closesocket((SOCKET)mSocket);
    mSocket = (uintptr_t)INVALID_SOCKET;
    mReceived.clear();
    if (mConnected.exchange(false)) {
        Logf("net: Anchor connection %u closed: %s", mConnectionId.load(), why);
    }
}

void NetService::Deliver(std::string&& data) {
    mPacketsIn++;
    std::lock_guard<std::mutex> lock(mMutex);
    const uint64_t seq = mNextSeq++;
    const uint32_t connection = mConnectionId.load();
    for (auto& [engine, queue] : mInbox) {
        if (queue.size() >= kMaxQueuedPerEngine) {
            queue.pop_front();
            if ((++mDropped % 1000) == 1) {
                // Only an engine whose Anchor never drains its queue gets here (its thread is not running).
                char line[160];
                snprintf(line, sizeof(line), "net: engine %d's Anchor queue is full; %zu old packets dropped", engine,
                         mDropped);
                if (mLog) {
                    mLog(line);
                }
            }
        }
        queue.push_back(Packet{ seq, connection, data });
    }
}

void NetService::Run() {
    auto backoff = std::chrono::milliseconds(500);
    char chunk[64 * 1024];

    while (true) {
        std::string address;
        uint16_t port = 0;
        uint32_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            if ((SOCKET)mSocket == INVALID_SOCKET) {
                mWake.wait(lock, [&] { return mQuit || mWanted; });
            }
            if (mQuit) {
                break;
            }
            if (!mWanted) {
                lock.unlock();
                CloseSocket("left the room");
                continue;
            }
            address = mAddress;
            port = mPort;
            generation = mWantGeneration;
        }

        if ((SOCKET)mSocket == INVALID_SOCKET) {
            if (!OpenSocket(address, port)) {
                std::unique_lock<std::mutex> lock(mMutex);
                mWake.wait_for(lock, backoff, [&] { return mQuit || mWantGeneration != generation; });
                backoff = std::min(backoff * 2, std::chrono::milliseconds(5000));
                continue;
            }
            backoff = std::chrono::milliseconds(500);
            std::string hello;
            bool stale;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                // Disconnected (or asked for elsewhere) while this connect was in flight?
                stale = !mWanted || mWantGeneration != generation;
                hello = mHello;
                mOutbox.clear(); // anything queued was for a connection that is gone
            }
            if (stale) {
                closesocket((SOCKET)mSocket);
                mSocket = (uintptr_t)INVALID_SOCKET;
                continue;
            }
            mReceived.clear();
            const uint32_t id = ++mConnectionId;
            if (id > 1) {
                mReconnects++;
            }
            mConnected = true;
            Logf("net: Anchor connected to %s:%u (connection %u%s)", address.c_str(), port, id,
                 id > 1 ? ", a reconnect" : "");
            if (!hello.empty()) {
                hello.push_back('\0');
                if (!SendAll((SOCKET)mSocket, hello.data(), hello.size())) {
                    CloseSocket("sending the hello failed");
                    continue;
                }
                mPacketsOut++;
            }
        }

        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mWanted || mWantGeneration != generation) {
                mOutbox.clear();
                // (CloseSocket below, outside the lock: it logs)
                generation = 0;
            }
        }
        if (generation == 0) {
            CloseSocket("left the room");
            continue;
        }

        std::deque<std::string> outgoing;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            outgoing.swap(mOutbox);
        }
        bool failed = false;
        for (std::string& packet : outgoing) {
            packet.push_back('\0');
            if (!SendAll((SOCKET)mSocket, packet.data(), packet.size())) {
                failed = true;
                break;
            }
            mPacketsOut++;
        }
        if (failed) {
            CloseSocket("send failed");
            continue;
        }

        fd_set readable;
        FD_ZERO(&readable);
        FD_SET((SOCKET)mSocket, &readable);
        timeval timeout = { 0, 4000 };
        const int ready = select((int)(SOCKET)mSocket + 1, &readable, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR) {
            CloseSocket("select failed");
            continue;
        }
        if (ready == 0) {
            continue;
        }
        const int received = recv((SOCKET)mSocket, chunk, sizeof(chunk), 0);
        if (received == 0) {
            CloseSocket("the server closed it");
            continue;
        }
        if (received == SOCKET_ERROR) {
            if (MS_WOULD_BLOCK()) {
                continue;
            }
            CloseSocket("receive failed");
            continue;
        }
        mReceived.append(chunk, (size_t)received);
        size_t start = 0;
        size_t end;
        while ((end = mReceived.find('\0', start)) != std::string::npos) {
            if (end > start) {
                Deliver(mReceived.substr(start, end - start));
            }
            start = end + 1;
        }
        mReceived.erase(0, start);
        if (mReceived.size() > kMaxPacketBytes) {
            CloseSocket("a packet without a delimiter grew too large");
        }
    }

    CloseSocket("mergeship.exe is exiting");
}

} // namespace MergeShipHost
