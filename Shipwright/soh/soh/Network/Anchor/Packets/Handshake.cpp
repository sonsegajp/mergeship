#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"

/**
 * HANDSHAKE
 *
 * Sent by the client to the server when it first connects to the server, sends over both the local room settings
 * in case the room needs to be created, along with the current client state
 */

// MergeShip2: built apart from sending, because on mergeship.exe's shared connection the host stores it and sends it
// first on every (re)connect (Network::PrepHello).
nlohmann::json Anchor::PrepHandshake() {
    nlohmann::json payload;
    payload["type"] = HANDSHAKE;
    payload["roomId"] = CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), "");
    payload["roomState"] = PrepRoomState();
    payload["clientState"] = PrepClientState();
    payload["clientId"] = ownClientId;
    return payload;
}

std::string Anchor::PrepHello() {
    return PrepHandshake().dump();
}

void Anchor::SendPacket_Handshake() {
    SendJsonToRemote(PrepHandshake());
}
