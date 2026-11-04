#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/JsonConversions.hpp"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/Rando/Rando.h"
#include <nlohmann/json.hpp>

extern "C" {
#include "variables.h"
extern PlayState* gPlayState;
}

/**
 * UPDATE_CLIENT_STATE
 *
 * Contains a small subset of data that is cached on the server and important for the client to know for various reasons
 *
 * Sent on various events, such as changing scenes, soft resetting, finishing the game, opening file select, etc.
 *
 * Note: This packet should be cross version compatible, so if you add anything here don't assume all clients will be
 * providing it, consider doing a `contains` check before accessing any version specific data
 *
 * A Termina player: "world" "mm", sceneNum MS_ANCHOR_MM_SCENE_BASE + Majora's Mask's scene (so a stock SoH client
 * never takes it for a Hyrule scene), "sceneName" (for Hyrule's room list), "cycle" and "form".
 */

nlohmann::json Anchor::PrepClientState() {
    nlohmann::json payload;
    payload["name"] = CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "");
    payload["color"] = CVarGetColor24(CVAR_REMOTE_ANCHOR("Color.Value"), { 100, 255, 100 });
    payload["clientVersion"] = clientVersion;
    payload["teamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["online"] = true;
    payload["world"] = MS_ANCHOR_WORLD_MM;

    if (IsSaveLoaded()) {
        payload["seed"] = IS_RANDO ? gSaveContext.save.shipSaveInfo.rando.finalSeed : 0;
        payload["isSaveLoaded"] = true;
        payload["isGameComplete"] = false;
        payload["sceneNum"] = WireScene(gPlayState->sceneId);
        payload["curRoomNum"] = gPlayState->roomCtx.curRoom.num;
        payload["entranceIndex"] = gSaveContext.save.entrance;
        payload["sceneName"] = SceneName(gPlayState->sceneId);
        payload["cycle"] = MergeShipAnchor_Cycle();
        payload["form"] = GET_PLAYER(gPlayState)->transformation;
    } else {
        payload["seed"] = 0;
        payload["isSaveLoaded"] = false;
        payload["isGameComplete"] = false;
        payload["sceneNum"] = -1;
        payload["curRoomNum"] = -1;
        payload["entranceIndex"] = 0x00;
    }

    return payload;
}

void Anchor::SendPacket_UpdateClientState() {
    nlohmann::json payload;
    payload["type"] = UPDATE_CLIENT_STATE;
    payload["state"] = PrepClientState();

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateClientState(nlohmann::json payload) {
    uint32_t clientId = payload.at("clientId").get<uint32_t>();

    if (clients.contains(clientId)) {
        AnchorClient client = payload["state"].get<AnchorClient>();
        AnchorClient& known = clients[clientId];
        const bool worldChanged = known.world != client.world;
        known.clientId = clientId;
        known.name = client.name;
        known.color = client.color;
        known.clientVersion = client.clientVersion;
        known.teamId = client.teamId;
        known.online = client.online;
        known.seed = client.seed;
        known.isSaveLoaded = client.isSaveLoaded;
        known.isGameComplete = client.isGameComplete;
        known.sceneNum = client.sceneNum;
        known.curRoomNum = client.curRoomNum;
        known.entranceIndex = client.entranceIndex;
        known.world = client.world;
        known.sceneName = client.sceneName;
        known.cycle = client.cycle;
        if (worldChanged) {
            // A teammate came through the door (either way): his dummy is (not) Termina's to draw.
            known.form = client.form;
            shouldRefreshActors = true;
        }
    }
}
