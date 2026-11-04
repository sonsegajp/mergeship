#include "2s2h/MergeShip/MergeShipHost.h"
#include <nlohmann/json.hpp>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"

extern "C" {
#include "variables.h"
extern PlayState* gPlayState;
}

/**
 * REQUEST_TELEPORT
 *
 * Because we don't have all the necessary information to directly teleport to a player, we emit a request,
 * in which they will respond with a TELEPORT_TO packet, with the necessary information.
 */

void Anchor::SendPacket_RequestTeleport(uint32_t clientId) {
    if (!CanTeleportTo(clientId)) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = REQUEST_TELEPORT;
    payload["targetClientId"] = clientId;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_RequestTeleport(nlohmann::json payload) {
    if (!IsSaveLoaded() || !MergeShipAnchor_Claim(payload)) { // one answer per process
        return;
    }

    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    SendPacket_TeleportTo(clientId);
}

// Reusable function to check if teleporting to a client is allowed
bool Anchor::CanTeleportTo(uint32_t clientId) {
    // Teleporting is disabled
    if (roomState.teleportMode == 0) {
        return false;
    }

    // You're not loaded into a save
    if (!IsSaveLoaded()) {
        return false;
    }

    // The client doesn't exist
    if (clients.find(clientId) == clients.end()) {
        return false;
    }

    AnchorClient& client = clients[clientId];

    // The client is yourself
    if (client.self) {
        return false;
    }

    // The client isn't online or loaded into a save
    if (!client.online || !client.isSaveLoaded) {
        return false;
    }

    // The client is in Hyrule (its entrance is an Ocarina of Time one): reached through the host's trip there
    // (BUG-0036), from a Termina that came from a Hyrule file.
    if (!IsOwnWorld(client)) {
        std::string ownTeamId = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
        return MergeShipHost_CanReachHyrule() && !((roomState.teleportMode == 1) && (client.teamId != ownTeamId));
    }
    if (LocalScene(client) < 0) {
        return false;
    }

    // Teleporting to team only, but the client is not on your team
    std::string ownTeamId = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    if (roomState.teleportMode == 1 && client.teamId != ownTeamId) {
        return false;
    }

    // Problematic scenes for teleporting: the moon and the final battle, the Clock Tower's rooftop and its interior
    // (the prologue).
    const s16 scene = LocalScene(client);
    if (scene == SCENE_SOUGEN || scene == SCENE_LAST_LINK || scene == SCENE_LAST_DEKU || scene == SCENE_LAST_GORON ||
        scene == SCENE_LAST_ZORA || scene == SCENE_LAST_BS || scene == SCENE_OKUJOU || scene == SCENE_INSIDETOWER ||
        scene == SCENE_OPENINGDAN) {
        return false;
    }

    return true;
}
