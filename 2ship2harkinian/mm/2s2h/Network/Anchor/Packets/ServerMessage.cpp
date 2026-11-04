#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/BenGui/Notification.h"
#include <nlohmann/json.hpp>

/**
 * SERVER_MESSAGE
 */

void Anchor::HandlePacket_ServerMessage(nlohmann::json payload) {
    if (!MergeShipAnchor_Claim(payload)) { // shown once, by one world
        return;
    }
    Notification::Emit({
        .prefix = "Server:",
        .prefixColor = ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
        .message = payload.at("message").get<std::string>(),
    });
}
