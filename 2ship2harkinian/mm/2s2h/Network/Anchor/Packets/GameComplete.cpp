#include <nlohmann/json.hpp>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/BenGui/Notification.h"

#include <cstdlib>

static const std::string gameCompleteMessages[] = {
    "sealed Majora",    "saved Termina",         "stopped the moon",
    "beat the Moon",    "is the Hero of Time",   "has met with a terrible fate... not",
};

/**
 * GAME_COMPLETE
 */

void Anchor::SendPacket_GameComplete() {
    if (!IsSaveLoaded()) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = GAME_COMPLETE;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_GameComplete(nlohmann::json payload) {
    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    if (!clients.contains(clientId)) {
        return;
    }

    AnchorClient& anchorClient = clients[clientId];
    anchorClient.isGameComplete = true;
    if (!MergeShipAnchor_Claim(payload)) { // announced once, by one world
        return;
    }
    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    Notification::Emit({
        .prefix = isGlobalRoom ? "Someone" : anchorClient.name,
        .message = gameCompleteMessages[rand() % (sizeof(gameCompleteMessages) / sizeof(gameCompleteMessages[0]))],
    });
}
