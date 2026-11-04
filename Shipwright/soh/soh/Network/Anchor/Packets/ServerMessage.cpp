#include "soh/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include "soh/Notification/Notification.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"

/**
 * SERVER_MESSAGE
 */

void Anchor::HandlePacket_ServerMessage(nlohmann::json payload) {
    if (!MergeShipAnchor_Claim(payload)) { // MergeShip2: shown once, by one world
        return;
    }
    Notification::Emit({
        .prefix = "Server:",
        .prefixColor = ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
        .message = payload.at("message").get<std::string>(),
    });
}
