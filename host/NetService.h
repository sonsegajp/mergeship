// mergeship.exe's one network connection (include/mergeship/AnchorNet.h): the Anchor socket both engines share, so
// that crossing between Hyrule and Termina never leaves the Anchor room. See the header for the model; this is the
// Winsock implementation behind MsHostServices.net.
#pragma once

#include <mergeship/AnchorNet.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace MergeShipHost {

class NetService {
  public:
    using LogFn = std::function<void(const char*)>;

    NetService();
    ~NetService();

    // `engines`: the MsEngineId of every loaded engine (each gets its own receive queue).
    void Init(const std::vector<int32_t>& engines, LogFn log);
    // The MsHostNet table for MsHostServices.net; `host` (the void* the engines pass back) must be this object.
    const MsHostNet* Api() const {
        return &mApi;
    }
    void SetActiveEngine(int32_t engineId);
    MsNetStatus Status();
    void Shutdown();

    // MsHostNet
    int32_t Connect(const char* address, uint16_t port, const char* hello, uint32_t helloLength);
    void Disconnect();
    void SetHello(const char* hello, uint32_t length);
    int32_t Send(const char* data, uint32_t length);
    int32_t Receive(int32_t engineId, char* buffer, uint32_t capacity, uint64_t* seq, uint32_t* connectionId);
    int32_t Claim(uint64_t seq, int32_t engineId);
    void SetBlob(const char* key, const char* data, uint32_t length);
    int32_t GetBlob(const char* key, char* buffer, uint32_t capacity);

  private:
    struct Packet {
        uint64_t seq;
        uint32_t connectionId;
        std::string data;
    };

    void Run();
    bool OpenSocket(const std::string& address, uint16_t port);
    void CloseSocket(const char* why);
    void Deliver(std::string&& packet);
    void Logf(const char* format, ...);

    MsHostNet mApi = {};
    LogFn mLog;

    std::mutex mMutex;
    std::condition_variable mWake;
    std::thread mThread;
    bool mQuit = false;

    // Guarded by mMutex.
    bool mWanted = false;
    uint32_t mWantGeneration = 0; // bumped by Connect/Disconnect: the thread drops a connection of an older one
    std::string mAddress;
    uint16_t mPort = 0;
    std::string mHello;
    std::deque<std::string> mOutbox;
    std::map<int32_t, std::deque<Packet>> mInbox;
    size_t mDropped = 0;
    uint64_t mNextSeq = 1;
    std::unordered_map<uint64_t, int32_t> mClaims;
    std::map<std::string, std::string> mBlobs;
    int32_t mActiveEngine = 0;

    // Written by the network thread, read anywhere.
    std::atomic<bool> mConnected{ false };
    std::atomic<uint32_t> mConnectionId{ 0 };
    std::atomic<uint32_t> mReconnects{ 0 };
    std::atomic<uint64_t> mPacketsIn{ 0 };
    std::atomic<uint64_t> mPacketsOut{ 0 };

    // Network thread only.
    uintptr_t mSocket; // SOCKET
    std::string mReceived;
    bool mWsaStarted = false;
};

} // namespace MergeShipHost
