#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <iterator>

extern "C" {
#include "variables.h"
#include "macros.h"
}

/**
 * MM_UPDATE_DUNGEON_ITEMS (SoH: UPDATE_DUNGEON_ITEMS)
 *
 * The four temples' boss keys, maps and compasses, their small keys (as they are found and used) and their stray
 * fairies. All of them are Majora's Mask's three days' (a Song of Time takes them), so only a teammate in the same
 * cycle takes them over.
 */

void Anchor::SendPacket_MMUpdateDungeonItems() {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags) {
        return;
    }

    const Inventory& inventory = gSaveContext.save.saveInfo.inventory;
    nlohmann::json payload;
    payload["type"] = MM_UPDATE_DUNGEON_ITEMS;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["dungeonItems"] = std::vector<u8>(std::begin(inventory.dungeonItems), std::end(inventory.dungeonItems));
    payload["dungeonKeys"] = std::vector<s8>(std::begin(inventory.dungeonKeys), std::end(inventory.dungeonKeys));
    payload["strayFairies"] = std::vector<s8>(std::begin(inventory.strayFairies), std::end(inventory.strayFairies));
    payload["cycle"] = MergeShipAnchor_Cycle();

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_MMUpdateDungeonItems(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }
    if (payload.value("cycle", (u16)0xFFFF) != MergeShipAnchor_Cycle()) {
        return; // another three days
    }

    Inventory& inventory = gSaveContext.save.saveInfo.inventory;
    std::vector<u8> items = payload.value("dungeonItems", std::vector<u8>{});
    std::vector<s8> keys = payload.value("dungeonKeys", std::vector<s8>{});
    std::vector<s8> fairies = payload.value("strayFairies", std::vector<s8>{});
    for (size_t i = 0; i < items.size() && i < ARRAY_COUNT(inventory.dungeonItems); i++) {
        inventory.dungeonItems[i] = items[i];
    }
    for (size_t i = 0; i < keys.size() && i < ARRAY_COUNT(inventory.dungeonKeys); i++) {
        inventory.dungeonKeys[i] = keys[i];
    }
    for (size_t i = 0; i < fairies.size() && i < ARRAY_COUNT(inventory.strayFairies); i++) {
        inventory.strayFairies[i] = fairies[i];
    }

    // Not sent back (PollGameState compares against these).
    lastDungeonItems.assign(std::begin(inventory.dungeonItems), std::end(inventory.dungeonItems));
    lastDungeonKeys.assign(std::begin(inventory.dungeonKeys), std::end(inventory.dungeonKeys));
    lastStrayFairies.assign(std::begin(inventory.strayFairies), std::end(inventory.strayFairies));
}
