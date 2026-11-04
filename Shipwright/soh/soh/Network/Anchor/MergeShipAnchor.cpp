// MergeShip2 -- Anchor players in Majora's Mask's transformation forms (MergeShipAnchor.h).
//
// Anchor mirrors a remote player with a dummy Player actor that never runs Player_Update: its fields are
// filled from PLAYER_UPDATE packets and it is drawn with Player_Draw. Ocarina of Time's Link needs only
// his age for that. A form needs more, because everything about it is decided per Player:
//   - the form itself: the dummy is rebuilt with MergeShip_ApplyFormChange (Player_InitCommon's form half:
//     the form's skeleton, its row of sAgeProperties, its shadow), and its hand, sheath and waist lists are
//     re-chosen from the form's column of the model tables (Player_SetModelGroup);
//   - its size: actor.scale, which the forms change (the Fierce Deity's 0.015, the Deku's squash in and out
//     of a flower, the Goron's roll), and its root motion, which MM scales by the form's own unk_08;
//   - what Player_Draw draws instead of or beside the skeleton, read from Player fields MM added: the Goron
//     ball (PLAYER_STATE3_1000, unk_ABC, unk_B86, unk_B28, unk_B10), the Deku flower (PLAYER_STATE3_100 /
//     40000, unk_ABC, actionVar2, unk_B86, unk_AF0), the Zora barrier and fins (unk_B62, unk_B8A-unk_B8E),
//     the Deku glide (PLAYER_STATE3_2000, unk_B8C-unk_B94), the mask on the face and the transformation's
//     glow (currentMask, prevMask, mergeShipTransforming), and which of MM's animations is playing (the
//     forms' draw code recognises them by path) for punches, instruments and the fins;
//   - the local player's own MM state the drawing reads (gMergeShipPlayerExt.mmBoots, and the Goron and
//     Zora trails and tire mark), which is swapped for the remote player's while the dummy is drawn.
// Colours: MergeShip_FormColors_SetForPlayer points the forms' colour segment at the remote player's
// Anchor colour for the dummy's draw (MergeShipFormColors.cpp), through the same VB_APPLY_TUNIC_COLOR Anchor
// already answers for Link's tunic.

#include "soh/Network/Anchor/MergeShipAnchor.h"

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include <libultraship/bridge/consolevariablebridge.h>

#include <cstring>
#include <map>
#include <memory>
#include <string>

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipFormColors.h"
#include "soh/MergeShip/MergeShipAdultMask.h" // MergeShip_AdultMask_DrawingKind (BUG-0057)
#include "soh/MergeShip/MergeShipMMEquipment.h" // MergeShip_MMEquip_SelectedBits (BUG-0077)
#include "soh/MergeShip/MergeShipPlayerExt.h"
#include "mm/objects/object_link_goron/object_link_goron.h"
extern PlayState* gPlayState;
// soh/MergeShip/MergeShipAnchorDraw.c (the form headers are C-only)
void MergeShipAnchor_SwapDrawState(u8 mmBoots);
void MergeShipAnchor_RestoreDrawState(void);
void MergeShip_ApplyFormChange(Player* this_, PlayState* play, u8 form);
void MergeShipTest_Log(const char* fmt, ...);
}

#define CVAR_ANCHOR_FORMS_NAME CVAR_ENHANCEMENT("MergeShip.AnchorForms")
#define CVAR_ANCHOR_FORMS_DEFAULT 1

namespace {

bool FormsEnabled() {
    return CVarGetInteger(CVAR_ANCHOR_FORMS_NAME, CVAR_ANCHOR_FORMS_DEFAULT) != 0;
}

// What a remote player's PLAYER_UPDATE said about its form, and what its dummy was last given.
struct RemoteForm {
    bool has = false;
    u8 form = PLAYER_FORM_CHILD;
    bool hidden = false;
    Vec3f scale = { 0.01f, 0.01f, 0.01f };
    u32 stateFlags3 = 0;
    std::string animation;
    f32 curFrame = 0.0f;
    std::string upperAnimation;
    f32 upperCurFrame = 0.0f;
    f32 unk_ABC = 0.0f;
    s16 unk_B86[2] = { 0, 0 };
    s16 unk_B28 = 0;
    f32 unk_B10[6] = {};
    Vec3f unk_AF0 = {};
    s16 unk_B62 = 0;
    s16 unk_B8A = 0, unk_B8C = 0, unk_B8E = 0, unk_B90 = 0, unk_B92 = 0, unk_B94 = 0;
    s16 actionVar2 = 0;
    s16 unk_6C2 = 0;
    f32 unk_6C4 = 0.0f;
    u8 currentMask = 0;
    u8 prevMask = 0;
    u8 transforming = 0;
    u8 adultMask = 0; // BUG-0057: MergeShip_AdultMask_DrawingKind
    u32 mmEquip = 0;  // BUG-0077: MergeShip_MMEquip_SelectedBits
    bool hasMMEquip = false;
    u8 mmBoots = MM_BOOTS_HYLIAN;
    f32 velocityY = 0.0f;
    f32 speedXZ = 0.0f;

    // The dummy's copies (only written in DummyPlayer_Update, so the pointers the draw reads stay valid
    // whenever packets arrive).
    std::string appliedAnimation;
    std::string appliedUpperAnimation;
    // The Goron's guard skeleton (MM Player::unk_2C8) needs tables of its own for every dummy.
    Vec3s guardJointTable[LINK_GORON_SHIELDING_LIMB_MAX] = {};
    Vec3s guardMorphTable[LINK_GORON_SHIELDING_LIMB_MAX] = {};
    bool guardInitialised = false;
};

std::map<uint32_t, std::unique_ptr<RemoteForm>> sRemoteForms;

RemoteForm& Remote(uint32_t clientId) {
    auto& slot = sRemoteForms[clientId];
    if (slot == nullptr) {
        slot = std::make_unique<RemoteForm>();
    }
    return *slot;
}

// Player animations are resource paths; only those are sent (the forms' draw code compares paths).
std::string AnimationPath(const void* animation) {
    const char* path = (const char*)animation;
    if ((path == nullptr) || (strncmp(path, "__OTR__", 7) != 0)) {
        return std::string();
    }
    return std::string(path);
}

u8 TargetForm(const RemoteForm& remote, s32 linkAge) {
    if (FormsEnabled() && remote.has && MERGESHIP_FORM_IS_MM(remote.form)) {
        return remote.form;
    }
    return (u8)linkAge;
}

} // namespace

// ---- packets -------------------------------------------------------------------------------------------

void MergeShipAnchor_WritePlayerUpdate(nlohmann::json& payload, Player* player) {
    if (!FormsEnabled() || (player == nullptr)) {
        return;
    }

    nlohmann::json ms;
    ms["form"] = player->mergeShipForm;
    ms["hidden"] = (player->actor.draw == nullptr);
    ms["scale"] = player->actor.scale;
    ms["stateFlags3"] = player->stateFlags3;
    ms["unk_6C2"] = player->unk_6C2;
    ms["unk_6C4"] = player->unk_6C4;
    ms["currentMask"] = player->currentMask;
    ms["prevMask"] = player->prevMask;
    ms["transforming"] = player->mergeShipTransforming;
    ms["adultMask"] = MergeShip_AdultMask_DrawingKind(player);
    ms["mmEquip"] = MergeShip_MMEquip_SelectedBits();
    if (MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        ms["anim"] = AnimationPath(player->skelAnime.animation);
        ms["frame"] = player->skelAnime.curFrame;
        ms["upperAnim"] = AnimationPath(player->upperSkelAnime.animation);
        ms["upperFrame"] = player->upperSkelAnime.curFrame;
        ms["unk_ABC"] = player->unk_ABC;
        ms["unk_B86"] = { player->unk_B86[0], player->unk_B86[1] };
        ms["unk_B28"] = player->unk_B28;
        ms["unk_B10"] = { player->unk_B10[0], player->unk_B10[1], player->unk_B10[2],
                          player->unk_B10[3], player->unk_B10[4], player->unk_B10[5] };
        ms["unk_AF0"] = player->unk_AF0[0];
        ms["unk_B62"] = player->unk_B62;
        ms["unk_B8A"] = { player->unk_B8A, player->unk_B8C, player->unk_B8E,
                          player->unk_B90, player->unk_B92, player->unk_B94 };
        ms["actionVar2"] = player->av2.actionVar2;
        ms["mmBoots"] = gMergeShipPlayerExt.mmBoots;
        ms["velocityY"] = player->actor.velocity.y;
        ms["speedXZ"] = player->actor.speedXZ;
    }
    payload["mergeShip"] = ms;
}

void MergeShipAnchor_ReadPlayerUpdate(uint32_t clientId, const nlohmann::json& payload) {
    RemoteForm& r = Remote(clientId);

    if (!payload.contains("mergeShip") || !payload["mergeShip"].is_object()) {
        r.has = false;
        return;
    }
    const nlohmann::json& ms = payload["mergeShip"];
    u8 form = ms.value("form", (u8)PLAYER_FORM_CHILD);

    r.has = true;
    r.form = (form < PLAYER_FORM_MAX) ? form : (u8)PLAYER_FORM_CHILD;
    r.hidden = ms.value("hidden", false);
    r.scale = ms.value("scale", Vec3f{ 0.01f, 0.01f, 0.01f });
    r.stateFlags3 = ms.value("stateFlags3", (u32)0);
    r.unk_6C2 = ms.value("unk_6C2", (s16)0);
    r.unk_6C4 = ms.value("unk_6C4", 0.0f);
    r.currentMask = ms.value("currentMask", (u8)0);
    r.prevMask = ms.value("prevMask", (u8)0);
    r.transforming = ms.value("transforming", (u8)0);
    r.adultMask = ms.value("adultMask", (u8)0);
    r.hasMMEquip = ms.contains("mmEquip");
    r.mmEquip = ms.value("mmEquip", (u32)0);
    r.animation = ms.value("anim", std::string());
    r.curFrame = ms.value("frame", 0.0f);
    r.upperAnimation = ms.value("upperAnim", std::string());
    r.upperCurFrame = ms.value("upperFrame", 0.0f);
    r.unk_ABC = ms.value("unk_ABC", 0.0f);
    r.unk_B28 = ms.value("unk_B28", (s16)0);
    r.unk_AF0 = ms.value("unk_AF0", Vec3f{ 0.0f, 0.0f, 0.0f });
    r.unk_B62 = ms.value("unk_B62", (s16)0);
    r.actionVar2 = ms.value("actionVar2", (s16)0);
    r.mmBoots = ms.value("mmBoots", (u8)MM_BOOTS_HYLIAN);
    r.velocityY = ms.value("velocityY", 0.0f);
    r.speedXZ = ms.value("speedXZ", 0.0f);

    std::vector<s16> b86 = ms.value("unk_B86", std::vector<s16>{});
    b86.resize(2, 0);
    r.unk_B86[0] = b86[0];
    r.unk_B86[1] = b86[1];
    std::vector<f32> b10 = ms.value("unk_B10", std::vector<f32>{});
    b10.resize(6, 0.0f);
    for (s32 i = 0; i < 6; i++) {
        r.unk_B10[i] = b10[i];
    }
    std::vector<s16> b8a = ms.value("unk_B8A", std::vector<s16>{});
    b8a.resize(6, 0);
    r.unk_B8A = b8a[0];
    r.unk_B8C = b8a[1];
    r.unk_B8E = b8a[2];
    r.unk_B90 = b8a[3];
    r.unk_B92 = b8a[4];
    r.unk_B94 = b8a[5];
}

// ---- the dummy -----------------------------------------------------------------------------------------

static void MergeShipAnchor_SetForm(Player* dummy, PlayState* play, u8 form, s32 linkAge) {
    s32 originalAge = gSaveContext.linkAge;

    // Player code asks gSaveContext for the age, as DummyPlayer_Init's own hack does.
    gSaveContext.linkAge = linkAge;
    MergeShip_ApplyFormChange(dummy, play, form);
    gSaveContext.linkAge = originalAge;
    // A form has its own column of the model tables: choose the hands, sheath and waist again.
    dummy->modelGroup = 0xFF;
    MergeShipTest_Log("ANCHOR dummy form=%d (age %d)", form, linkAge);
}

void MergeShipAnchor_DummyInit(Player* dummy, PlayState* play, uint32_t clientId, s32 linkAge) {
    RemoteForm& r = Remote(clientId);
    u8 form = TargetForm(r, linkAge);

    r.guardInitialised = false;
    if (form != dummy->mergeShipForm) {
        MergeShipAnchor_SetForm(dummy, play, form, linkAge);
    }
}

void MergeShipAnchor_DummyUpdate(Player* dummy, PlayState* play, uint32_t clientId, s32 linkAge) {
    RemoteForm& r = Remote(clientId);
    u8 form = TargetForm(r, linkAge);

    if (form != dummy->mergeShipForm) {
        MergeShipAnchor_SetForm(dummy, play, form, linkAge);
    }
    if (!FormsEnabled() || !r.has) {
        return;
    }

    dummy->actor.scale = r.scale;
    dummy->unk_6C2 = r.unk_6C2;
    dummy->unk_6C4 = r.unk_6C4;
    dummy->currentMask = r.currentMask;
    dummy->prevMask = r.prevMask;
    dummy->mergeShipTransforming = r.transforming;
    MergeShip_AdultMask_SetRemote(dummy, r.adultMask);
    dummy->stateFlags3 = r.stateFlags3;

    if (!MERGESHIP_FORM_IS_MM(dummy->mergeShipForm)) {
        return;
    }

    // The forms' draw code recognises MM's animations by their path.
    r.appliedAnimation = r.animation;
    r.appliedUpperAnimation = r.upperAnimation;
    if (!r.appliedAnimation.empty()) {
        dummy->skelAnime.animation = (LinkAnimationHeader*)r.appliedAnimation.c_str();
    }
    dummy->skelAnime.curFrame = r.curFrame;
    if (!r.appliedUpperAnimation.empty()) {
        dummy->upperSkelAnime.animation = (LinkAnimationHeader*)r.appliedUpperAnimation.c_str();
    }
    dummy->upperSkelAnime.curFrame = r.upperCurFrame;

    dummy->unk_ABC = r.unk_ABC;
    dummy->unk_B86[0] = r.unk_B86[0];
    dummy->unk_B86[1] = r.unk_B86[1];
    dummy->unk_B28 = r.unk_B28;
    for (s32 i = 0; i < 6; i++) {
        dummy->unk_B10[i] = r.unk_B10[i];
    }
    dummy->unk_AF0[0] = r.unk_AF0;
    dummy->unk_B62 = r.unk_B62;
    dummy->unk_B8A = r.unk_B8A;
    dummy->unk_B8C = r.unk_B8C;
    dummy->unk_B8E = r.unk_B8E;
    dummy->unk_B90 = r.unk_B90;
    dummy->unk_B92 = r.unk_B92;
    dummy->unk_B94 = r.unk_B94;
    dummy->av2.actionVar2 = r.actionVar2;
    dummy->actor.velocity.y = r.velocityY;
    dummy->actor.speedXZ = r.speedXZ;
    // The afterimages and the thrown boomerang belong to the remote game; nothing is left to trail them here.
    dummy->unk_3D0.unk_00 = 0;
    dummy->zoraBoomerangActor = NULL;
    dummy->heldActor = NULL;

    // MM Player_InitCommon's Goron guard skeleton (unk_2C8), animated as MM's Goron guard action does.
    if (dummy->mergeShipForm == PLAYER_FORM_GORON) {
        if (!r.guardInitialised) {
            SkelAnime_InitFlex(play, &dummy->unk_2C8, (FlexSkeletonHeader*)mm_gLinkGoronShieldingSkel,
                               (AnimationHeader*)mm_gLinkGoronShieldingAnim, r.guardJointTable, r.guardMorphTable,
                               LINK_GORON_SHIELDING_LIMB_MAX);
            r.guardInitialised = true;
        }
        if (dummy->stateFlags1 & PLAYER_STATE1_SHIELDING) {
            SkelAnime_Update(&dummy->unk_2C8);
        }
    }
}

f32 MergeShipAnchor_DummyRootScaleXZ(Player* dummy, s32 linkAge) {
    // Player_ApplyAnimMovementScaledByAge: MM scales a form's root motion by the form's own unk_08.
    if (dummy->mergeShipForm >= PLAYER_FORM_DEKU) {
        return dummy->ageProperties->unk_08;
    }
    return (linkAge == LINK_AGE_CHILD) ? 0.64f : 1.0f;
}

bool MergeShipAnchor_DummyBeginDraw(Player* dummy, PlayState* play, uint32_t clientId) {
    RemoteForm& r = Remote(clientId);

    MergeShip_FormColors_SetForPlayer(play, dummy);

    // BUG-0077: his MM sword, shield, bow and hookshot (the sword from his B button, swapped in by DummyPlayer_Draw).
    if (r.has && r.hasMMEquip) {
        dummy->currentSwordItemId = gSaveContext.equips.buttonItems[0];
        MergeShip_MMEquip_SetDrawOverride(true, r.mmEquip);
    }

    if (!FormsEnabled() || !r.has) {
        return true;
    }
    if (r.hidden) {
        return false;
    }
    if (!MERGESHIP_FORM_IS_MM(dummy->mergeShipForm)) {
        return true;
    }

    // The forms' drawing reads some of the local player's MM state; the dummy's is the remote player's.
    MergeShipAnchor_SwapDrawState(r.mmBoots);
    return true;
}

void MergeShipAnchor_DummyEndDraw(Player* dummy, PlayState* play) {
    MergeShip_MMEquip_SetDrawOverride(false, 0);
    MergeShipAnchor_RestoreDrawState();
    // Everything drawn after this is the local player's again.
    MergeShip_FormColors_SetForPlayer(play, GET_PLAYER(play));
}

// ---- loopback test -------------------------------------------------------------------------------------

namespace {
const uint32_t kLoopbackClientId = 0x4D530001; // "MS" 1
f32 sLoopbackOffsetX = 0.0f;
f32 sLoopbackOffsetZ = 0.0f;
} // namespace

void Anchor::MergeShipLoopbackStart(Color_RGB8 color, f32 offsetX, f32 offsetZ) {
    if (!IsSaveLoaded()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    AnchorClient client = {};

    sLoopbackOffsetX = offsetX;
    sLoopbackOffsetZ = offsetZ;
    client.clientId = kLoopbackClientId;
    client.name = "Loopback";
    client.color = color;
    client.clientVersion = clientVersion;
    client.teamId = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    client.online = true;
    client.self = false;
    client.isSaveLoaded = true;
    client.sceneNum = gPlayState->sceneNum;
    client.linkAge = gSaveContext.linkAge;
    client.posRot = player->actor.world;
    clients[kLoopbackClientId] = client;

    mergeShipLoopback = true;
    RegisterHooks();
    MergeShipLoopbackFrame();
    RefreshClientActors();
    MergeShipTest_Log("ANCHOR loopback on color=(%d,%d,%d) offset=(%.0f,%.0f)", color.r, color.g, color.b, offsetX,
                      offsetZ);
}

// Called from the OnPlayerUpdate hook: this frame's PLAYER_UPDATE, as another client would receive it.
void Anchor::MergeShipLoopbackFrame() {
    if (!mergeShipLoopback || !IsSaveLoaded() || !clients.contains(kLoopbackClientId)) {
        return;
    }
    nlohmann::json payload = PrepPlayerUpdate();

    payload["clientId"] = kLoopbackClientId;
    payload["posRot"]["pos"]["x"] = payload["posRot"]["pos"]["x"].get<f32>() + sLoopbackOffsetX;
    payload["posRot"]["pos"]["z"] = payload["posRot"]["pos"]["z"].get<f32>() + sLoopbackOffsetZ;
    // Through text, as it would travel.
    HandlePacket_PlayerUpdate(nlohmann::json::parse(payload.dump()));
    clients[kLoopbackClientId].online = true;
    clients[kLoopbackClientId].isSaveLoaded = true;
}

extern "C" void MergeShipAnchor_TestLoopback(u8 r, u8 g, u8 b, f32 dx, f32 dz) {
    if (Anchor::Instance != nullptr) {
        Anchor::Instance->MergeShipLoopbackStart(Color_RGB8{ r, g, b }, dx, dz);
    }
}
