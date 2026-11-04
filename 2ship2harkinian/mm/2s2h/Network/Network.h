#pragma once

// MergeShip2 -- 2 Ship 2 Harkinian's network base for Anchor, ported from Ship of Harkinian's soh/Network/Network.h.
//
// Standalone (2ship.exe) it owns an SDL_net socket, exactly as SoH's does. In mergeship.exe it is a view of the
// host's one connection (include/mergeship/AnchorNet.h), which Hyrule's engine shares: crossing between the worlds
// never disconnects.

#ifdef __cplusplus

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

#if defined(MS_ANCHOR_SDLNET)
#include <SDL2/SDL_net.h>
#endif

struct MsHostNet;

class Network {
  private:
#if defined(MS_ANCHOR_SDLNET)
    IPaddress networkAddress;
    TCPsocket networkSocket = nullptr;
#endif
    std::thread receiveThread;
    std::string receivedData;

    void ReceiveFromServer();
    void HandleRemoteData(char payload[512]);

    // The host's connection (AnchorNet.h): see SoH's Network.h.
    const MsHostNet* hostNet = nullptr;
    void* hostContext = nullptr;
    int32_t hostEngineId = 0;
    uint32_t hostConnectionId = 0;
    std::atomic<bool> hostLinkRunning{ false };
    std::thread hostLinkThread;
    void HostLink();

  protected:
    void HandleRemoteJson(std::string payload, uint64_t hostSeq = 0);
    // The HANDSHAKE packet, stored by the host and sent first on every (re)connect. Empty: none.
    virtual std::string PrepHello() {
        return std::string();
    }

  public:
    bool isEnabled = false;
    bool isConnected = false;

    void Enable(const char* host, uint16_t port);
    void Disable();
    virtual void OnIncomingData(char payload[512]);
    // Called for each complete JSON packet (packets are delimited by a NUL).
    virtual void OnIncomingJson(nlohmann::json payload);
    virtual void OnConnected();
    virtual void OnDisconnected();
    virtual void ProcessOutgoingPackets();
    void SendDataToRemote(const char* payload);
    virtual void SendJsonToRemote(nlohmann::json packet);
    // The connection's first packet. Own socket: sent now. Host transport: the hello for later reconnects.
    void SendHelloToRemote(nlohmann::json packet);

    // Be a view of the host's shared connection from now on (idempotent). engineId: MsEngineId.
    void UseHostTransport(const MsHostNet* net, void* host, int32_t engineId);
    bool IsHostTransport() const {
        return hostNet != nullptr;
    }
    uint32_t HostConnectionId() const {
        return hostConnectionId;
    }
};

#endif // __cplusplus
