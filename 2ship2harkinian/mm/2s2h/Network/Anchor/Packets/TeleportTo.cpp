#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/JsonConversions.hpp"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include <nlohmann/json.hpp>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
}

/**
 * TELEPORT_TO
 *
 * See REQUEST_TELEPORT for more information, this is the second part of the process.
 */

void Anchor::SendPacket_TeleportTo(uint32_t clientId) {
    if (!IsSaveLoaded()) {
        return;
    }

    Player* player = GET_PLAYER(gPlayState);

    nlohmann::json payload;
    payload["type"] = TELEPORT_TO;
    payload["world"] = MS_ANCHOR_WORLD_MM;
    payload["targetClientId"] = clientId;
    payload["entranceIndex"] = gSaveContext.save.entrance;
    payload["roomIndex"] = gPlayState->roomCtx.curRoom.num;
    payload["posRot"] = player->actor.world;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_TeleportTo(nlohmann::json payload) {
    if (!IsSaveLoaded()) {
        return;
    }
    // A Hyrule player's answer: an Ocarina of Time entrance -- the host's trip there (BUG-0036).
    if (payload.value("world", std::string(MS_ANCHOR_WORLD_OOT)) != MS_ANCHOR_WORLD_MM) {
        PosRot target = payload.at("posRot").get<PosRot>();
        const f32 pos[3] = { target.pos.x, target.pos.y, target.pos.z };
        MergeShipHost_RequestHyruleAnchorWarp(payload.at("entranceIndex").get<s32>(), payload.at("roomIndex").get<s32>(),
                                              pos, target.rot.y);
        return;
    }

    s32 entranceIndex = payload.at("entranceIndex").get<s32>();
    s8 roomIndex = payload.at("roomIndex").get<s8>();

    if (entranceIndex < 0 || roomIndex < 0) {
        SPDLOG_ERROR("[Anchor] TELEPORT_TO: invalid entranceIndex {} or roomIndex {}", entranceIndex, (int)roomIndex);
        return;
    }

    PosRot posRot = payload.at("posRot").get<PosRot>();

    // As a void-out respawn (RESPAWN_MODE_DOWN, respawnFlag 1), at the teammate's position
    Play_SetRespawnData(gPlayState, RESPAWN_MODE_DOWN, (u16)entranceIndex, roomIndex,
                        PLAYER_PARAMS(0xFF, PLAYER_START_MODE_B), &posRot.pos, posRot.rot.y);
    gPlayState->nextEntrance = (u16)entranceIndex;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_INSTANT;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
    gSaveContext.respawnFlag = 1;

    // ... without the void-out's damage (Player_Init arms it with unk_D6A; SoH suppresses VB_INFLICT_VOID_DAMAGE).
    static HOOK_ID hookId = 0;
    hookId = GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorInit>(
        ACTOR_PLAYER, [](Actor* actor) {
            if (gPlayState != nullptr && actor == (Actor*)GET_PLAYER(gPlayState)) {
                ((Player*)actor)->unk_D6A = 0;
                GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnActorInit>(hookId);
            }
        });
}
