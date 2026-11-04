#pragma once

#ifdef __cplusplus

#include <atomic>
#include <thread>
#include <SDL2/SDL_net.h>
#include <nlohmann/json.hpp>

struct MsHostNet;

class Network {
  private:
    IPaddress networkAddress;
    TCPsocket networkSocket;
    std::thread receiveThread;
    std::string receivedData;

    void ReceiveFromServer();
    void HandleRemoteData(char payload[512]);

    // MergeShip2: mergeship.exe's one connection (include/mergeship/AnchorNet.h). With a host transport this object
    // owns no socket: it is a view of the host's connection, which both engines share, and a link thread (running
    // from UseHostTransport on, whether this engine is active or suspended) mirrors the host's state into isEnabled /
    // isConnected and drains this engine's queue of received packets.
    const MsHostNet* hostNet = nullptr;
    void* hostContext = nullptr;
    int32_t hostEngineId = 0;
    uint32_t hostConnectionId = 0;
    std::atomic<bool> hostLinkRunning{ false };
    std::thread hostLinkThread;
    void HostLink();

  protected:
    void HandleRemoteJson(std::string payload, uint64_t hostSeq = 0);
    // MergeShip2: the HANDSHAKE packet, stored by the host and sent first on every (re)connect. Empty: none.
    virtual std::string PrepHello() {
        return std::string();
    }

  public:
    bool isEnabled;
    bool isConnected;

    void Enable(const char* host, uint16_t port);
    void Disable();
    /**
     * Raw data handler
     *
     * If you are developing a new remote, you should probably use the json methods instead. This
     * method requires you to parse the data and ensure packets are complete manually, we cannot
     * gaurentee that the data will be complete, or that it will only contain one packet with this
     */
    virtual void OnIncomingData(char payload[512]);
    /**
     * Json handler
     *
     * This method will be called when a complete json packet is received. All json packets must
     * be delimited by a null terminator (\0).
     */
    virtual void OnIncomingJson(nlohmann::json payload);
    virtual void OnConnected();
    virtual void OnDisconnected();
    virtual void ProcessOutgoingPackets();
    void SendDataToRemote(const char* payload);
    virtual void SendJsonToRemote(nlohmann::json packet);
    // MergeShip2: the connection's first packet. Own socket: sent now. Host transport: stored as the hello for later
    // reconnects (the host already sent one on this connection).
    void SendHelloToRemote(nlohmann::json packet);

    // MergeShip2: be a view of the host's shared connection from now on (idempotent). engineId: MsEngineId.
    void UseHostTransport(const MsHostNet* net, void* host, int32_t engineId);
    bool IsHostTransport() const {
        return hostNet != nullptr;
    }
    const MsHostNet* HostNet() const {
        return hostNet;
    }
    void* HostContext() const {
        return hostContext;
    }
    int32_t HostEngineId() const {
        return hostEngineId;
    }
    // The host connection's number (1, 2, ...; the same in both engines and across world switches), 0 without one.
    uint32_t HostConnectionId() const {
        return hostConnectionId;
    }
};

#endif // __cplusplus
