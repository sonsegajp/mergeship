#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/JsonConversions.hpp"
#include <nlohmann/json.hpp>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "2s2h/MergeShip/MergeShipAdultLink.h"
#include "2s2h/MergeShip/MergeShipAdultMask.h" // MergeShip_AdultMask_DrawingKind (BUG-0057)
#include "2s2h/MergeShip/MergeShipGanondorf.h" // MergeShip2: Ganondorf mode
extern PlayState* gPlayState;
}

/**
 * PLAYER_UPDATE
 *
 * Contains real-time data necessary to update other clients in the same scene as the player
 *
 * Sent every frame to other clients within the same scene
 *
 * Note: This packet is sent _a lot_, so please do not include any unnecessary data in it
 *
 * Majora's Mask's player: its form, its mask and what Player_Draw reads for the forms (the Goron ball, the Deku flower
 * and glide, the Zora fins and barrier). Only Termina players read it ("world": "mm").
 */

void Anchor::SendPacket_PlayerUpdate() {
    if (!IsSaveLoaded()) {
        return;
    }

    uint32_t currentPlayerCount = 0;
    for (auto& [clientId, client] : clients) {
        if (IsInOwnScene(client) && client.online && client.isSaveLoaded && !client.self) {
            currentPlayerCount++;
        }
    }
    if (currentPlayerCount == 0) {
        return;
    }

    nlohmann::json payload = PrepPlayerUpdate();

    for (auto& [clientId, client] : clients) {
        if (IsInOwnScene(client) && client.online && client.isSaveLoaded && !client.self) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
    }
}

nlohmann::json Anchor::PrepPlayerUpdate() {
    Player* player = GET_PLAYER(gPlayState);
    nlohmann::json payload;

    payload["type"] = PLAYER_UPDATE;
    payload["world"] = MS_ANCHOR_WORLD_MM;
    payload["sceneNum"] = WireScene(gPlayState->sceneId);
    payload["entranceIndex"] = gSaveContext.save.entrance;
    payload["form"] = player->transformation;
    payload["humanAdult"] = MergeShip_AdultLink_TablesAdult() ? 1 : 0; // MergeShip2: the ghost's age
    payload["msGanondorf"] = MergeShip_Ganondorf_AnchorValue(); // MergeShip2: they see Ganondorf (and his form) too
    payload["msGanondorfArmor"] = MergeShip_Ganondorf_ArmorColor(); // MergeShip2: in his colours
    payload["msGanondorfCape"] = MergeShip_Ganondorf_CapeColor();
    {
        // MergeShip2 (BUG-0111): Beast Ganon's own skeleton's animation and frame, so the others see him
        f32 beastFrame = 0.0f;
        payload["msBeastAnim"] = MergeShip_Ganondorf_AnchorBeast(&beastFrame);
        payload["msBeastFrame"] = beastFrame;
    }
    payload["posRot"]["pos"] = player->actor.world.pos;
    payload["posRot"]["rot"] = player->actor.shape.rot;
    std::vector<int> jointArray;
    for (size_t i = 0; i < ANCHOR_JOINT_COUNT; i++) {
        Vec3s joint = player->skelAnime.jointTable[i];
        jointArray.push_back(joint.x);
        jointArray.push_back(joint.y);
        jointArray.push_back(joint.z);
    }
    payload["prevTransl"] = player->skelAnime.prevTransl;
    payload["movementFlags"] = player->skelAnime.movementFlags;
    payload["jointTable"] = jointArray;
    payload["upperLimbRot"] = player->upperLimbRot;
    payload["headLimbRot"] = player->headLimbRot;
    payload["currentBoots"] = player->currentBoots;
    payload["currentShield"] = player->currentShield;
    payload["currentMask"] = player->currentMask;
    payload["adultMask"] = MergeShip_AdultMask_DrawingKind(player); // MergeShip2 (BUG-0057)
    {
        // MergeShip2 (BUG-0077): the Ocarina of Time equipment he wears
        const MergeShipOoTEquipSave& oot = gSaveContext.save.shipSaveInfo.mergeShipOoTEquip;
        payload["ootEquip"] = { oot.sword, oot.shield, oot.tunic, oot.boots, oot.strength, oot.bgsFlag, oot.swordHealth };
    }
    payload["prevMask"] = player->prevMask;
    payload["stateFlags1"] = player->stateFlags1;
    payload["stateFlags2"] = player->stateFlags2;
    payload["stateFlags3"] = player->stateFlags3;
    payload["buttonItemB"] = CUR_FORM_EQUIP(EQUIP_SLOT_B);
    payload["equipment"] = gSaveContext.save.saveInfo.equips.equipment;
    payload["itemAction"] = player->itemAction;
    payload["heldItemAction"] = player->heldItemAction;
    payload["heldItemId"] = player->heldItemId;
    payload["modelGroup"] = player->modelGroup;
    payload["invincibilityTimer"] = player->invincibilityTimer;
    payload["actionVar1"] = player->av1.actionVar1;
    payload["actionVar2"] = player->av2.actionVar2;
    payload["unk_ABC"] = player->unk_ABC;
    payload["unk_B86"] = { player->unk_B86[0], player->unk_B86[1] };
    payload["unk_B28"] = player->unk_B28;
    payload["unk_B10"] = { player->unk_B10[0], player->unk_B10[1], player->unk_B10[2],
                           player->unk_B10[3], player->unk_B10[4], player->unk_B10[5] };
    payload["arr_AF0"] = { player->arr_AF0[0], player->arr_AF0[1], player->arr_AF0[2],
                           player->arr_AF0[3], player->arr_AF0[4], player->arr_AF0[5] };
    payload["unk_B62"] = player->unk_B62;
    payload["unk_B8A"] = { player->unk_B8A, player->unk_B8C, player->unk_B8E,
                           player->unk_B90, player->unk_B92, player->unk_B94 };
    payload["scale"] = player->actor.scale;
    payload["speedXZ"] = player->actor.speed;
    payload["velocityY"] = player->actor.velocity.y;
    payload["quiet"] = true;

    return payload;
}

void Anchor::HandlePacket_PlayerUpdate(nlohmann::json payload) {
    uint32_t clientId = payload["clientId"].get<uint32_t>();

    if (!clients.contains(clientId)) {
        return;
    }
    auto& client = clients[clientId];

    client.world = payload.value("world", std::string(MS_ANCHOR_WORLD_OOT));
    client.sceneNum = payload.value("sceneNum", (s16)-1);
    client.entranceIndex = payload.value("entranceIndex", (s32)0);
    if (!IsOwnWorld(client)) {
        // A Hyrule player (Ocarina of Time's Link; SoH draws him). Only where he is matters here.
        return;
    }

    const u8 form = payload.value("form", (u8)PLAYER_FORM_HUMAN);
    if (client.form != form && client.player != nullptr) {
        shouldRefreshActors = true; // another form, another skeleton
    }
    client.form = form;
    // MergeShip2: the human's age is its skeleton too (MergeShipAdultLink.h): another age, another ghost
    client.ganondorf = payload.value("msGanondorf", (u8)0); // MergeShip2: Ganondorf mode (MergeShipGanondorf.h)
    client.ganondorfArmor = payload.value("msGanondorfArmor", (u32)0);
    client.ganondorfCape = payload.value("msGanondorfCape", (u32)0);
    client.ganondorfBeastAnim = payload.value("msBeastAnim", (s8)-1);
    client.ganondorfBeastFrame = payload.value("msBeastFrame", 0.0f);
    const u8 humanAdult = payload.value("humanAdult", (u8)0);
    if (client.humanAdult != humanAdult && client.player != nullptr && form == PLAYER_FORM_HUMAN) {
        shouldRefreshActors = true;
    }
    client.humanAdult = humanAdult;
    client.posRot = payload.value("posRot", PosRot{});
    std::vector<int> jointArray = payload.value("jointTable", std::vector<int>{});
    jointArray.resize(ANCHOR_JOINT_COUNT * 3); // Ensure it has enough elements, in case of missing data
    for (int i = 0; i < ANCHOR_JOINT_COUNT; i++) {
        client.jointTable[i].x = jointArray[i * 3];
        client.jointTable[i].y = jointArray[i * 3 + 1];
        client.jointTable[i].z = jointArray[i * 3 + 2];
    }
    client.movementFlags = payload.value("movementFlags", (u8)0);
    client.prevTransl = payload.value("prevTransl", Vec3s{ 0 });
    client.upperLimbRot = payload.value("upperLimbRot", Vec3s{ 0 });
    client.headLimbRot = payload.value("headLimbRot", Vec3s{ 0 });
    client.currentBoots = payload.value("currentBoots", (s8)0);
    client.currentShield = payload.value("currentShield", (s8)0);
    client.currentMask = payload.value("currentMask", (u8)0);
    client.adultMask = payload.value("adultMask", (u8)0);
    client.hasOoTEquip = payload.contains("ootEquip") && payload["ootEquip"].is_array() && (payload["ootEquip"].size() >= 7);
    if (client.hasOoTEquip) {
        const nlohmann::json& oot = payload["ootEquip"];
        client.ootSword = oot[0].get<u8>();
        client.ootShield = oot[1].get<u8>();
        client.ootTunic = oot[2].get<u8>();
        client.ootBoots = oot[3].get<u8>();
        client.ootStrength = oot[4].get<u8>();
        client.ootBgsFlag = oot[5].get<u8>();
        client.ootSwordHealth = oot[6].get<s16>();
    }
    client.prevMask = payload.value("prevMask", (u8)0);
    client.stateFlags1 = payload.value("stateFlags1", (u32)0);
    client.stateFlags2 = payload.value("stateFlags2", (u32)0);
    client.stateFlags3 = payload.value("stateFlags3", (u32)0);
    client.buttonItemB = payload.value("buttonItemB", (u8)ITEM_NONE);
    client.equipment = payload.value("equipment", (u16)0);
    client.itemAction = payload.value("itemAction", (s8)0);
    client.heldItemAction = payload.value("heldItemAction", (s8)0);
    client.heldItemId = payload.value("heldItemId", (u8)ITEM_NONE);
    client.modelGroup = payload.value("modelGroup", (u8)0);
    client.invincibilityTimer = payload.value("invincibilityTimer", (s8)0);
    client.actionVar1 = payload.value("actionVar1", (s8)0);
    client.actionVar2 = payload.value("actionVar2", (s16)0);
    client.unk_ABC = payload.value("unk_ABC", 0.0f);
    client.unk_B28 = payload.value("unk_B28", (s16)0);
    client.unk_B62 = payload.value("unk_B62", (s16)0);
    client.scale = payload.value("scale", Vec3f{ 0.01f, 0.01f, 0.01f });
    client.speedXZ = payload.value("speedXZ", 0.0f);
    client.velocityY = payload.value("velocityY", 0.0f);

    std::vector<s16> b86 = payload.value("unk_B86", std::vector<s16>{});
    b86.resize(2, 0);
    client.unk_B86[0] = b86[0];
    client.unk_B86[1] = b86[1];
    std::vector<f32> b10 = payload.value("unk_B10", std::vector<f32>{});
    b10.resize(6, 0.0f);
    std::vector<f32> af0 = payload.value("arr_AF0", std::vector<f32>{});
    af0.resize(6, 0.0f);
    std::vector<s16> b8a = payload.value("unk_B8A", std::vector<s16>{});
    b8a.resize(6, 0);
    for (int i = 0; i < 6; i++) {
        client.unk_B10[i] = b10[i];
        client.arr_AF0[i] = af0[i];
        client.unk_B8A[i] = b8a[i];
    }
}
