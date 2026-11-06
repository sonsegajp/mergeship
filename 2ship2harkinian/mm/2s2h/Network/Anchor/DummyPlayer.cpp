// MergeShip2 -- another Anchor player in Termina, ported from Ship of Harkinian's DummyPlayer.cpp.
//
// Majora's Mask's own Player, built as Kafei (En_Test3) is built: play->playerInit (Player_InitCommon) on the
// remote player's form's skeleton and row of properties, never updated by Player_Update: its fields come from
// PLAYER_UPDATE packets and it is drawn with Player_Draw. A form is the Player's own (skeleton, colliders, the Goron
// guard skeleton), so a remote player who changes form is spawned again (Anchor::RefreshClientActors).
#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/NameTag/NameTag.h"

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "z64malloc.h"
#include "2s2h/MergeShip/MergeShipAdultLink.h"
#include "2s2h/MergeShip/MergeShipAdultMask.h" // BUG-0057
extern PlayState* gPlayState;
extern PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX]; // ovl_player_actor/z_player.c
void Player_Draw(Actor* thisx, PlayState* play);                   // ovl_player_actor/z_player.c
// MergeShip2 (BUG-0136): an Anchor player's horse (ovl_En_Horse/z_en_horse.c)
Actor* EnHorse_MergeShipSpawnPuppet(PlayState* play, const Vec3f* riderPos, s16 yaw);
void EnHorse_MergeShipPlacePuppet(Actor* horse, PlayState* play, const Vec3f* riderPos, s16 yaw, s32 adult, s32 ganon);
}
#include <unordered_map>

// MergeShip2 (BUG-0136): the horse each remote player rides (EnHorse_MergeShipSpawnPuppet), by their client
static std::unordered_map<uint32_t, Actor*> sDummyHorses;

// (still this scene's -- its memory is not a freed horse's or another actor's: it moves from the BG list to the MISC
// list at its first update)
static bool DummyPlayer_HorseIsLive(PlayState* play, Actor* horse) {
    for (s32 category : { ACTORCAT_BG, ACTORCAT_MISC }) {
        for (Actor* actor = play->actorCtx.actorLists[category].first; actor != nullptr; actor = actor->next) {
            if (actor == horse) {
                return actor->id == ACTOR_EN_HORSE;
            }
        }
    }
    return false;
}

static void DummyPlayer_RemoveHorse(PlayState* play, uint32_t clientId) {
    auto it = sDummyHorses.find(clientId);

    if (it == sDummyHorses.end()) {
        return;
    }
    if ((play != nullptr) && DummyPlayer_HorseIsLive(play, it->second)) {
        Actor_Kill(it->second);
    }
    sDummyHorses.erase(it);
}

// Majora's Mask's 32 damage types (z64collision_check.h DMG_*); the effects are the dummy's own values.
static DamageTable DummyPlayerDamageTable = { {
    /* Deku Nut       */ DMG_ENTRY(0, DUMMY_PLAYER_HIT_RESPONSE_STUN),
    /* Deku Stick     */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Horse trample  */ DMG_ENTRY(0, 0),
    /* Explosives     */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Zora boomerang */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Normal arrow   */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, 0),
    /* Hookshot       */ DMG_ENTRY(0, DUMMY_PLAYER_HIT_RESPONSE_STUN),
    /* Goron punch    */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Sword          */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Goron pound    */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Fire arrow     */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_FIRE),
    /* Ice arrow      */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Light arrow    */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Goron spikes   */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Deku spin      */ DMG_ENTRY(0, DUMMY_PLAYER_HIT_RESPONSE_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Deku launch    */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, 0),
    /* Zora barrier   */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_STUN),
    /* Normal shield  */ DMG_ENTRY(0, 0),
    /* Light ray      */ DMG_ENTRY(0, 0),
    /* Thrown object  */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Zora punch     */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Spin attack    */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Sword beam     */ DMG_ENTRY(4, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Normal Roll    */ DMG_ENTRY(0, 0),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, 0),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, 0),
    /* Unblockable    */ DMG_ENTRY(0, 0),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, 0),
    /* Powder Keg     */ DMG_ENTRY(4, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
} };

namespace {

// Player code reads the form and the equipped sword/shield from the save (as SoH's dummy reads the age): while the
// dummy is built or drawn, the save says what the remote player has.
struct RemoteSave {
    u8 playerForm;
    ItemEquips equips;
    MergeShipOoTEquipSave ootEquip; // MergeShip2 (BUG-0077): his Ocarina of Time equipment, as he wears it

    RemoteSave(const AnchorClient& client) {
        playerForm = gSaveContext.save.playerForm;
        equips = gSaveContext.save.saveInfo.equips;
        ootEquip = gSaveContext.save.shipSaveInfo.mergeShipOoTEquip;
        if (client.hasOoTEquip) {
            MergeShipOoTEquipSave& oot = gSaveContext.save.shipSaveInfo.mergeShipOoTEquip;
            oot.sword = client.ootSword;
            oot.shield = client.ootShield;
            oot.tunic = client.ootTunic;
            oot.boots = client.ootBoots;
            oot.strength = client.ootStrength;
            oot.bgsFlag = client.ootBgsFlag;
            oot.swordHealth = client.ootSwordHealth;
        }
        const u8 form = (client.form < PLAYER_FORM_MAX) ? client.form : PLAYER_FORM_HUMAN;
        gSaveContext.save.playerForm = form;
        const u8 formIndex = (form == PLAYER_FORM_HUMAN) ? 0 : form;
        gSaveContext.save.saveInfo.equips.buttonItems[formIndex][EQUIP_SLOT_B] = client.buttonItemB;
        gSaveContext.save.saveInfo.equips.equipment = client.equipment;
    }
    ~RemoteSave() {
        gSaveContext.save.playerForm = playerForm;
        gSaveContext.save.saveInfo.equips = equips;
        gSaveContext.save.shipSaveInfo.mergeShipOoTEquip = ootEquip;
    }
};

u8 ClientForm(const AnchorClient& client) {
    return (client.form < PLAYER_FORM_MAX) ? client.form : (u8)PLAYER_FORM_HUMAN;
}

// Kafei's (z_en_test3.c sBlureInit, sTireMarkInit): the sword trails and the tire mark Player's code reads by index
EffectBlureInit2 sDummyBlureInit = {
    0,
    EFFECT_BLURE_ELEMENT_FLAG_8,
    0,
    { 255, 255, 255, 255 },
    { 255, 255, 255, 64 },
    { 255, 255, 255, 0 },
    { 255, 255, 255, 0 },
    4,
    0,
    EFF_BLURE_DRAW_MODE_SMOOTH,
    0,
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

EffectTireMarkInit sDummyTireMarkInit = {
    0,
    63,
    { 0, 0, 15, 100 },
};

} // namespace

// MergeShip2: whether a remote player (a dummy) has Ganondorf mode on (MergeShipGanondorf.h), from their updates:
// 0 off, else 1 + his form. (SoH's, for Termina's players.)
extern "C" s32 MergeShipAnchor_DummyIsGanondorf(const Actor* actor) {
    if ((Anchor::Instance == nullptr) || (actor == nullptr)) {
        return 0;
    }
    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);
    if (!Anchor::Instance->clients.contains(clientId)) {
        return 0;
    }
    return Anchor::Instance->clients[clientId].ganondorf;
}

// MergeShip2 (BUG-0111): a remote Beast Ganon's animation and frame
extern "C" void MergeShipAnchor_DummyGanondorfBeast(const Actor* actor, s32* index, f32* frame) {
    *index = -1;
    *frame = 0.0f;
    if ((actor == nullptr) || (Anchor::Instance == nullptr)) {
        return;
    }
    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);
    if (!Anchor::Instance->clients.contains(clientId)) {
        return;
    }
    *index = Anchor::Instance->clients[clientId].ganondorfBeastAnim;
    *frame = Anchor::Instance->clients[clientId].ganondorfBeastFrame;
}

// MergeShip2: Ganondorf mode -- a remote Ganondorf's colours (0: as they are)
extern "C" u32 MergeShipAnchor_DummyGanondorfColor(const Actor* actor, s32 cape) {
    if ((actor == nullptr) || (Anchor::Instance == nullptr)) {
        return 0;
    }
    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);
    if (!Anchor::Instance->clients.contains(clientId)) {
        return 0;
    }
    return cape ? Anchor::Instance->clients[clientId].ganondorfCape : Anchor::Instance->clients[clientId].ganondorfArmor;
}

void DummyPlayer_Init(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);

    if (!Anchor::Instance->clients.contains(clientId)) {
        Actor_Kill(actor);
        return;
    }

    AnchorClient& client = Anchor::Instance->clients[clientId];
    const u8 form = ClientForm(client);

    // #region modeled after EnTest3_Init (Kafei) and Player_Init
    actor->room = -1;
    player->csId = CS_ID_NONE;
    player->transformation = form;
    player->ageProperties = &sPlayerAgeProperties[form];
    player->itemAction = player->heldItemAction = PLAYER_IA_NONE;
    player->heldItemId = ITEM_NONE;
    player->currentMask = client.currentMask;
    player->prevMask = client.prevMask;
    // As EnTest3_Init: Player_SetAction (func_18780) marks the tire mark, and the draw adds to the trails
    Effect_Add(play, &player->meleeWeaponEffectIndex[0], EFFECT_BLURE2, 0, 0, &sDummyBlureInit);
    Effect_Add(play, &player->meleeWeaponEffectIndex[1], EFFECT_BLURE2, 0, 0, &sDummyBlureInit);
    Effect_Add(play, &player->meleeWeaponEffectIndex[2], EFFECT_TIRE_MARK, 0, 0, &sDummyTireMarkInit);
    {
        RemoteSave remote(client);
        // MergeShip2: the human is built at the remote player's age (its skeleton and rows), whatever ours is
        const s32 ownAdult = MergeShip_AdultLink_UseTables(client.humanAdult);
        Player_SetModelGroup(player, PLAYER_MODELGROUP_DEFAULT);
        play->playerInit(player, play, gPlayerSkeletons[form]);
        // The mask on the face is drawn from the mask object's segment (as Kafei's)
        player->maskObjectSegment = ZeldaArena_Malloc(0x3800);
        play->func_18780(player, play);
        MergeShip_AdultLink_UseTables(ownAdult);
    }
    // #endregion

    player->cylinder.base.acFlags = AC_ON | AC_TYPE_PLAYER;
    player->cylinder.base.ocFlags2 = OC2_TYPE_1;
    player->cylinder.elem.acElemFlags = ACELEM_ON | ACELEM_HOOKABLE | ACELEM_NO_HITMARK;
    player->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    player->cylinder.dim.radius = 30;
    player->actor.colChkInfo.damageTable = &DummyPlayerDamageTable;

    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    if (!isGlobalRoom) {
        NameTagOptions options = {};
        options.textColor = { client.color.r, client.color.g, client.color.b, 255 };
        NameTag_RegisterForActorWithOptions(actor, client.name.c_str(), options);
    }
}

// Update the actor with new data from the client
void DummyPlayer_Update(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);

    if (!Anchor::Instance->clients.contains(clientId)) {
        Actor_Kill(actor);
        return;
    }

    AnchorClient& client = Anchor::Instance->clients[clientId];

    if (!client.online || !client.isSaveLoaded || !Anchor::Instance->IsInOwnScene(client)) {
        actor->world.pos.x = -9999.0f;
        actor->world.pos.y = -9999.0f;
        actor->world.pos.z = -9999.0f;
        actor->shape.shadowAlpha = 0;
        DummyPlayer_RemoveHorse(play, clientId); // MergeShip2 (BUG-0136)
        return;
    }

    // Another form: another skeleton. Built again, next frame.
    if (ClientForm(client) != player->transformation) {
        Anchor::Instance->RequestActorRefresh();
        actor->shape.shadowAlpha = 0;
        return;
    }

    actor->shape.shadowAlpha = 255;
    player->upperLimbRot = client.upperLimbRot;
    player->headLimbRot = client.headLimbRot;
    actor->shape.rot = client.posRot.rot;
    actor->world.pos = client.posRot.pos;
    actor->scale = client.scale;
    player->skelAnime.jointTable = client.jointTable;
    player->skelAnime.movementFlags = client.movementFlags;
    player->skelAnime.prevTransl = client.prevTransl;
    player->currentBoots = client.currentBoots;
    player->currentShield = client.currentShield;
    player->currentMask = client.currentMask;
    // MergeShip2 (BUG-0158): the worn mask's object, as Player_Update readies it (func_801229FC: Player_Draw draws a
    // mask only once maskId has caught up with currentMask) -- a teammate's Bremen Mask, Bunny Hood ... were never drawn.
    func_801229FC(player);
    MergeShip_AdultMask_SetRemote(player, client.adultMask); // MergeShip2 (BUG-0057)
    player->prevMask = client.prevMask;
    player->stateFlags1 = client.stateFlags1;
    player->stateFlags2 = client.stateFlags2;
    player->stateFlags3 = client.stateFlags3;
    player->itemAction = client.itemAction;
    player->heldItemAction = client.heldItemAction;
    player->heldItemId = client.heldItemId;
    player->invincibilityTimer = client.invincibilityTimer;
    player->av1.actionVar1 = client.actionVar1;
    player->av2.actionVar2 = client.actionVar2;
    // What Player_Draw reads for the forms: the Goron ball, the Deku flower and glide, the Zora fins and barrier.
    player->unk_ABC = client.unk_ABC;
    player->unk_B86[0] = client.unk_B86[0];
    player->unk_B86[1] = client.unk_B86[1];
    player->unk_B28 = client.unk_B28;
    for (s32 i = 0; i < 6; i++) {
        player->unk_B10[i] = client.unk_B10[i];
        player->arr_AF0[i] = client.arr_AF0[i];
    }
    player->unk_B62 = client.unk_B62;
    player->unk_B8A = client.unk_B8A[0];
    player->unk_B8C = client.unk_B8A[1];
    player->unk_B8E = client.unk_B8A[2];
    player->unk_B90 = client.unk_B8A[3];
    player->unk_B92 = client.unk_B8A[4];
    player->unk_B94 = client.unk_B8A[5];
    player->actor.speed = client.speedXZ;
    player->actor.velocity.y = client.velocityY;
    // The afterimages and the thrown boomerangs belong to the remote game; nothing is left to trail them here.
    player->unk_3D0.unk_00 = 0;
    player->zoraBoomerangActor = NULL;
    player->heldActor = NULL;
    player->meleeWeaponState = 0;

    // Apply animation movement (as Player_Anim_ApplyMovement)
    Vec3f diff;
    SkelAnime_UpdateTranslation(&player->skelAnime, &diff, player->actor.shape.rot.y);

    if (player->skelAnime.movementFlags & ANIM_FLAG_1) {
        diff.x *= player->ageProperties->unk_08;
        diff.z *= player->ageProperties->unk_08;
        player->actor.world.pos.x += diff.x * player->actor.scale.x;
        player->actor.world.pos.z += diff.z * player->actor.scale.z;
    }

    if (player->skelAnime.movementFlags & ANIM_FLAG_UPDATE_Y) {
        if (!(player->skelAnime.movementFlags & ANIM_FLAG_4)) {
            diff.y *= player->ageProperties->unk_08;
        }
        player->actor.world.pos.y += diff.y * player->actor.scale.y;
    }

    // MergeShip2 (BUG-0136): while they ride, their horse under them -- Ganondorf's horse in Ganondorf mode, Ocarina of
    // Time's adult Epona for the adult, young Epona for the young (as EnHorse_Init picks for this game's player)
    if (player->stateFlags1 & PLAYER_STATE1_800000) {
        Actor* horse = nullptr;
        auto it = sDummyHorses.find(clientId);

        if ((it != sDummyHorses.end()) && DummyPlayer_HorseIsLive(play, it->second)) {
            horse = it->second;
        } else {
            horse = EnHorse_MergeShipSpawnPuppet(play, &player->actor.world.pos, player->actor.shape.rot.y);
            if (horse != nullptr) {
                sDummyHorses[clientId] = horse;
            } else {
                sDummyHorses.erase(clientId);
            }
        }
        if (horse != nullptr) {
            s32 ganon = client.ganondorf != 0;

            EnHorse_MergeShipPlacePuppet(horse, play, &player->actor.world.pos, player->actor.shape.rot.y,
                                         ganon || client.humanAdult, ganon);
        }
    } else {
        DummyPlayer_RemoveHorse(play, clientId);
    }

    if (player->modelGroup != client.modelGroup) {
        RemoteSave remote(client);
        Player_SetModelGroup(player, (PlayerModelGroup)client.modelGroup);
    }

    // The Goron's guard (MM Player_InitCommon's unk_2C8), animated as the guard action does.
    if ((player->transformation == PLAYER_FORM_GORON) && (player->stateFlags1 & PLAYER_STATE1_400000)) {
        SkelAnime_Update(&player->unk_2C8);
    }

    if (Anchor::Instance->roomState.pvpMode == 0 ||
        (Anchor::Instance->roomState.pvpMode == 1 &&
         client.teamId == CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default"))) {
        actor->flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
        return;
    }

    actor->flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;

    if ((player->cylinder.base.acFlags & AC_HIT) && player->invincibilityTimer == 0) {
        Anchor::Instance->SendPacket_DamagePlayer(client.clientId, player->actor.colChkInfo.damageEffect,
                                                  player->actor.colChkInfo.damage);
        if (player->actor.colChkInfo.damageEffect == DUMMY_PLAYER_HIT_RESPONSE_STUN) {
            Actor_SetColorFilter(&player->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_OPA, 24);
        } else {
            player->invincibilityTimer = 20;
        }
    }

    Collider_UpdateCylinder(&player->actor, &player->cylinder);

    if (!(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_2000 | PLAYER_STATE1_4000 |
                                 PLAYER_STATE1_800000))) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &player->cylinder.base);
    }

    if (!(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_4000000)) && (player->invincibilityTimer <= 0)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &player->cylinder.base);

        if (player->invincibilityTimer < 0) {
            CollisionCheck_SetAT(play, &play->colChkCtx, &player->cylinder.base);
        }
    }

    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000)) {
        player->actor.colChkInfo.mass = MASS_IMMOVABLE;
    } else {
        player->actor.colChkInfo.mass = 50;
    }

    Collider_ResetCylinderAC(play, &player->cylinder.base);
}

void DummyPlayer_Draw(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);

    if (!Anchor::Instance->clients.contains(clientId)) {
        Actor_Kill(actor);
        return;
    }

    AnchorClient& client = Anchor::Instance->clients[clientId];

    if (!client.online || !client.isSaveLoaded || !Anchor::Instance->IsInOwnScene(client) ||
        ClientForm(client) != player->transformation) {
        return;
    }

    RemoteSave remote(client);
    // MergeShip2: drawn with the rows of the remote player's age (face, hands, sword), then ours again
    const s32 ownAdult = MergeShip_AdultLink_UseTables(client.humanAdult);
    Player_Draw((Actor*)player, play);
    MergeShip_AdultLink_UseTables(ownAdult);
}

void DummyPlayer_Destroy(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    // MergeShip2 (BUG-0136): their horse goes with them (if it is still here: a scene's end frees it first)
    if (Anchor::Instance != nullptr) {
        DummyPlayer_RemoveHorse(play, Anchor::Instance->GetDummyPlayerClientId(actor));
    }

    if (player->maskObjectSegment != NULL) {
        ZeldaArena_Free(player->maskObjectSegment);
        player->maskObjectSegment = NULL;
    }
    Effect_Destroy(play, player->meleeWeaponEffectIndex[0]);
    Effect_Destroy(play, player->meleeWeaponEffectIndex[1]);
    Effect_Destroy(play, player->meleeWeaponEffectIndex[2]);
    NameTag_RemoveAllForActor(actor);
    // The dummy was spawned as ACTOR_PLAYER (and counted as one); be one again for Actor_Delete.
    actor->id = ACTOR_PLAYER;
}
