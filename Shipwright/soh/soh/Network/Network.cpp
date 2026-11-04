#include "Network.h"
#include <spdlog/spdlog.h>
#include <mergeship/AnchorNet.h>

#include <chrono>
#include <vector>

// MARK: - Public

void Network::Enable(const char* host, uint16_t port) {
    // MergeShip2: the host's shared connection (AnchorNet.h); the link thread mirrors its state.
    if (hostNet != nullptr) {
        std::string hello = PrepHello();
        hostNet->Connect(hostContext, host, port, hello.empty() ? nullptr : hello.c_str(), (uint32_t)hello.size());
        isEnabled = true;
        return;
    }

    if (isEnabled) {
        return;
    }

    if (SDLNet_ResolveHost(&networkAddress, host, port) == -1) {
        SPDLOG_ERROR("[Network] SDLNet_ResolveHost: {}", SDLNet_GetError());
    }

    isEnabled = true;

    // First check if there is a thread running, if so, join it
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    receiveThread = std::thread(&Network::ReceiveFromServer, this);
}

void Network::Disable() {
    // MergeShip2: leaving the room leaves it for both engines (the host's one connection).
    if (hostNet != nullptr) {
        hostNet->Disconnect(hostContext);
        isEnabled = false;
        return;
    }

    if (!isEnabled) {
        return;
    }

    isEnabled = false;
    receiveThread.join();
}

void Network::OnIncomingData(char payload[512]) {
}

void Network::OnIncomingJson(nlohmann::json payload) {
}

void Network::OnConnected() {
}

void Network::OnDisconnected() {
}

void Network::ProcessOutgoingPackets() {
}

void Network::SendDataToRemote(const char* payload) {
    SPDLOG_DEBUG("[Network] Sending data: {}", payload);
    if (hostNet != nullptr) {
        // MergeShip2: the host adds the NUL delimiter.
        hostNet->Send(hostContext, payload, static_cast<uint32_t>(strlen(payload)));
        return;
    }
    SDLNet_TCP_Send(networkSocket, payload, static_cast<int>(strlen(payload) + 1));
}

void Network::SendJsonToRemote(nlohmann::json payload) {
    SendDataToRemote(payload.dump().c_str());
}

void Network::SendHelloToRemote(nlohmann::json payload) {
    if (hostNet != nullptr) {
        std::string hello = payload.dump();
        hostNet->SetHello(hostContext, hello.c_str(), static_cast<uint32_t>(hello.size()));
        return;
    }
    Network::SendJsonToRemote(payload);
}

// MARK: - MergeShip2 host transport

void Network::UseHostTransport(const MsHostNet* net, void* host, int32_t engineId) {
    if (hostNet != nullptr || net == nullptr) {
        return;
    }
    // An own connection opened before the host's was offered (it should not happen: the engine attaches before its
    // saved "Enabled" CVar is read) is closed first.
    if (isEnabled) {
        Disable();
    }
    hostNet = net;
    hostContext = host;
    hostEngineId = engineId;
    hostLinkRunning = true;
    hostLinkThread = std::thread(&Network::HostLink, this);
}

// Runs for the rest of the process, suspended engine or not: this engine's view of the host connection.
void Network::HostLink() {
    std::vector<char> buffer(64 * 1024);

    while (hostLinkRunning) {
        MsNetStatus status = {};
        status.size = sizeof(status);
        hostNet->GetStatus(hostContext, &status);

        isEnabled = status.enabled != 0;
        const bool up = status.enabled != 0 && status.connected != 0;
        if (up && (!isConnected || status.connectionId != hostConnectionId)) {
            if (isConnected) {
                // The host reconnected underneath us (the server or the network dropped it).
                isConnected = false;
                OnDisconnected();
            }
            hostConnectionId = status.connectionId;
            isConnected = true;
            SPDLOG_INFO("[Network] Host connection {} is up", hostConnectionId);
            OnConnected();
        } else if (!up && isConnected) {
            isConnected = false;
            SPDLOG_INFO("[Network] Host connection {} is down", hostConnectionId);
            OnDisconnected();
        }

        bool received = false;
        for (int i = 0; i < 512; i++) {
            uint64_t seq = 0;
            uint32_t connection = 0;
            const int32_t length = hostNet->Receive(hostContext, hostEngineId, buffer.data(),
                                                    static_cast<uint32_t>(buffer.size()), &seq, &connection);
            if (length == 0) {
                break;
            }
            if (length < 0) {
                buffer.resize(static_cast<size_t>(-length));
                continue;
            }
            received = true;
            HandleRemoteJson(std::string(buffer.data(), static_cast<size_t>(length)), seq);
        }

        if (isConnected) {
            ProcessOutgoingPackets();
        }
        if (!received) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

// MARK: - Private

void Network::ReceiveFromServer() {
    while (isEnabled) {
        while (!isConnected && isEnabled) {
            SPDLOG_TRACE("[Network] Attempting to make connection to server...");
            networkSocket = SDLNet_TCP_Open(&networkAddress);

            if (networkSocket) {
                isConnected = true;
                receivedData.clear();
                SPDLOG_INFO("[Network] Connection to server established!");

                OnConnected();
                break;
            }
        }

        SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(1);
        if (networkSocket) {
            SDLNet_TCP_AddSocket(socketSet, networkSocket);
        }

        // Listen to socket messages
        while (isConnected && networkSocket && isEnabled) {
            // we check first if socket has data, to not block in the TCP_Recv
            int socketsReady = SDLNet_CheckSockets(socketSet, 0);

            if (socketsReady == -1) {
                SPDLOG_ERROR("[Network] SDLNet_CheckSockets: {}", SDLNet_GetError());
                break;
            }

            // Always process outgoing packets
            ProcessOutgoingPackets();

            if (socketsReady == 0) {
                // No incoming data
                continue;
            }

            char remoteDataReceived[512];
            memset(remoteDataReceived, 0, sizeof(remoteDataReceived));
            int len = SDLNet_TCP_Recv(networkSocket, &remoteDataReceived, sizeof(remoteDataReceived));
            if (!len || !networkSocket || len == -1) {
                SPDLOG_ERROR("[Network] SDLNet_TCP_Recv: {}", SDLNet_GetError());
                break;
            }

            HandleRemoteData(remoteDataReceived);

            receivedData.append(remoteDataReceived, len);

            // Proess all complete packets
            size_t delimiterPos = receivedData.find('\0');
            while (delimiterPos != std::string::npos) {
                // Extract the complete packet until the delimiter
                std::string packet = receivedData.substr(0, delimiterPos);
                // Remove the packet (including the delimiter) from the received data
                receivedData.erase(0, delimiterPos + 1);
                HandleRemoteJson(packet);
                // Find the next delimiter
                delimiterPos = receivedData.find('\0');
            }
        }

        if (socketSet) {
            SDLNet_FreeSocketSet(socketSet);
        }

        if (isConnected) {
            SDLNet_TCP_Close(networkSocket);
            networkSocket = nullptr;
            isConnected = false;
            receivedData.clear();
            OnDisconnected();
            SPDLOG_INFO("[Network] Ending receiving thread...");
        }
    }
}

void Network::HandleRemoteData(char payload[512]) {
    OnIncomingData(payload);
}

void Network::HandleRemoteJson(std::string payload, uint64_t hostSeq) {
    SPDLOG_DEBUG("[Network] Received json: {}", payload);
    nlohmann::json jsonPayload;
    try {
        jsonPayload = nlohmann::json::parse(payload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Failed to parse json: \n{}\n{}\n", payload, e.what());
        return;
    }
    // MergeShip2: the host's packet number, for MsHostNet.Claim (once-per-process packets).
    if (hostSeq != 0 && jsonPayload.is_object()) {
        jsonPayload[MS_ANCHOR_KEY_SEQ] = hostSeq;
    }

    try {
        OnIncomingJson(jsonPayload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Exception handling incoming JSON: {}", e.what());
    } catch (...) { SPDLOG_ERROR("[Network] Unknown exception handling incoming JSON"); }
}
