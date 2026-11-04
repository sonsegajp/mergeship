#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Notification/Notification.h"
#include "soh/ShipUtils.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"

const std::string gameCompleteMessages[] = {
    "killed Ganon",           "saved Zelda",         "proved their Courage",
    "collected the Triforce", "is the Hero of Time", "proved Mido wrong",
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
    if (!MergeShipAnchor_Claim(payload)) { // MergeShip2: announced once, by one world
        return;
    }
    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    Notification::Emit({
        .prefix = isGlobalRoom ? "Someone" : anchorClient.name,
        .message = ShipUtils::RandomElement(gameCompleteMessages),
    });
}
