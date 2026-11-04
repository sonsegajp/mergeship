#include "soh/Network/Anchor/Anchor.h"
std::string MergeShipAnchor_ClientIdOwnerOf(); // Anchor.cpp
#include "soh/Network/Anchor/JsonConversions.hpp"
#include <nlohmann/json.hpp>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include "soh/OTRGlobals.h"
#include "soh/Notification/Notification.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"

/**
 * ALL_CLIENT_STATE
 *
 * Contains a list of all clients and their CLIENT_STATE currently connected to the server
 *
 * The server itself sends this packet to all clients when a client connects or disconnects
 */

void Anchor::HandlePacket_AllClientState(nlohmann::json payload) {
    std::vector<AnchorClient> newClients = payload["state"].get<std::vector<AnchorClient>>();
    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));
    // MergeShip2: both worlds' engines keep the room's list, but only one of them announces who came and went.
    bool notify = !isGlobalRoom && MergeShipAnchor_Claim(payload);

    // add new clients
    for (auto& client : newClients) {
        if (client.self) {
            ownClientId = client.clientId;
            MergeShipAnchor_NoteOwnClientId(ownClientId); // MergeShip2: echoes of an old id are ours too
            CVarSetInteger(CVAR_REMOTE_ANCHOR("LastClientId"), ownClientId);
            // MergeShip2: whose id it is (Anchor.cpp MergeShipAnchor_ClientIdOwner)
            CVarSetString(CVAR_REMOTE_ANCHOR("LastClientIdOwner"), MergeShipAnchor_ClientIdOwnerOf().c_str());
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            clients[client.clientId].self = true;
        } else {
            clients[client.clientId].self = false;
            if (clients.contains(client.clientId)) {
                if (clients[client.clientId].online != client.online && notify) {
                    Notification::Emit({
                        .prefix = client.name,
                        .message = client.online ? "Connected" : "Disconnected",
                    });
                }
            } else if (client.online && notify) {
                Notification::Emit({
                    .prefix = client.name,
                    .message = "Connected",
                });
            }
        }

        clients[client.clientId].clientId = client.clientId;
        clients[client.clientId].name = client.name;
        clients[client.clientId].color = client.color;
        clients[client.clientId].clientVersion = client.clientVersion;
        clients[client.clientId].teamId = client.teamId;
        clients[client.clientId].online = client.online;
        clients[client.clientId].seed = client.seed;
        clients[client.clientId].isSaveLoaded = client.isSaveLoaded;
        clients[client.clientId].isGameComplete = client.isGameComplete;
        clients[client.clientId].sceneNum = client.sceneNum;
        clients[client.clientId].entranceIndex = client.entranceIndex;
        clients[client.clientId].world = client.world;         // MergeShip2
        clients[client.clientId].sceneName = client.sceneName; // MergeShip2
    }

    // remove clients that are no longer in the list
    std::vector<uint32_t> clientsToRemove;
    for (auto& [clientId, client] : clients) {
        if (std::find_if(newClients.begin(), newClients.end(),
                         [clientId](AnchorClient& c) { return c.clientId == clientId; }) == newClients.end()) {
            clientsToRemove.push_back(clientId);
        }
    }
    // (separate loop to avoid iterator invalidation)
    for (auto& clientId : clientsToRemove) {
        clients.erase(clientId);
    }

    shouldRefreshActors = true;
}
