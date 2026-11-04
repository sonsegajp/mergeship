#include "Anchor.h"
#include "soh/Enhancements/nametag.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"
#include "soh/MergeShip/MergeShipEpona.h"
#include <unordered_map>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;

void Player_UseItem(PlayState* play, Player* player, s32 item);
void Player_Draw(Actor* actor, PlayState* play);
}

static DamageTable DummyPlayerDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, DUMMY_PLAYER_HIT_RESPONSE_STUN),
    /* Deku stick    */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Slingshot     */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Explosive     */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Boomerang     */ DMG_ENTRY(0, DUMMY_PLAYER_HIT_RESPONSE_STUN),
    /* Normal arrow  */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Hammer swing  */ DMG_ENTRY(2, PLAYER_HIT_RESPONSE_KNOCKBACK_LARGE),
    /* Hookshot      */ DMG_ENTRY(0, DUMMY_PLAYER_HIT_RESPONSE_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Master sword  */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Giant's Knife */ DMG_ENTRY(4, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Fire arrow    */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, PLAYER_HIT_RESPONSE_FROZEN),
    /* Light arrow   */ DMG_ENTRY(2, PLAYER_HIT_RESPONSE_ELECTRIFIED),
    /* Unk arrow 1   */ DMG_ENTRY(2, PLAYER_HIT_RESPONSE_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, PLAYER_HIT_RESPONSE_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, PLAYER_HIT_RESPONSE_NONE),
    /* Fire magic    */ DMG_ENTRY(0, DUMMY_PLAYER_HIT_RESPONSE_FIRE),
    /* Ice magic     */ DMG_ENTRY(3, PLAYER_HIT_RESPONSE_FROZEN),
    /* Light magic   */ DMG_ENTRY(0, PLAYER_HIT_RESPONSE_ELECTRIFIED),
    /* Shield        */ DMG_ENTRY(0, PLAYER_HIT_RESPONSE_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, PLAYER_HIT_RESPONSE_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Giant spin    */ DMG_ENTRY(4, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Master spin   */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Kokiri jump   */ DMG_ENTRY(2, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Giant jump    */ DMG_ENTRY(8, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Master jump   */ DMG_ENTRY(4, DUMMY_PLAYER_HIT_RESPONSE_NORMAL),
    /* Unknown 1     */ DMG_ENTRY(0, PLAYER_HIT_RESPONSE_NONE),
    /* Unblockable   */ DMG_ENTRY(0, PLAYER_HIT_RESPONSE_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, PLAYER_HIT_RESPONSE_KNOCKBACK_LARGE),
    /* Unknown 2     */ DMG_ENTRY(0, PLAYER_HIT_RESPONSE_NONE),
};

// MergeShip2: whether a remote player (a dummy) has Ganondorf mode on (MergeShipGanondorf.h), from their updates:
// 0 off, else 1 + his form.
extern "C" s32 MergeShipAnchor_DummyIsGanondorf(const Actor* actor) {
    if ((Anchor::Instance == nullptr) || (actor == nullptr)) {
        return false;
    }
    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);
    if (!Anchor::Instance->clients.contains(clientId)) {
        return false;
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

// MergeShip2 (BUG-0136): the horse each remote player rides (EnHorse_MergeShipSpawnPuppet), by their client
static std::unordered_map<uint32_t, Actor*> sDummyHorses;

// (still this scene's: its memory is not a freed horse's or another actor's)
static bool DummyPlayer_HorseIsLive(PlayState* play, Actor* horse) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != nullptr; actor = actor->next) {
        if (actor == horse) {
            return actor->id == ACTOR_EN_HORSE;
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

// While the remote player rides, their horse under them: Ganondorf's horse in Ganondorf mode, the young Epona for young
// Link, Epona for the adult (as EnHorse_Init picks for this game's player).
static void DummyPlayer_UpdateHorse(Player* player, PlayState* play, AnchorClient& client) {
    if (!(player->stateFlags1 & PLAYER_STATE1_ON_HORSE)) {
        DummyPlayer_RemoveHorse(play, client.clientId);
        return;
    }
    Actor* horse = nullptr;
    auto it = sDummyHorses.find(client.clientId);

    if ((it != sDummyHorses.end()) && DummyPlayer_HorseIsLive(play, it->second)) {
        horse = it->second;
    } else {
        horse = EnHorse_MergeShipSpawnPuppet(play, &player->actor.world.pos, player->actor.shape.rot.y);
        if (horse == nullptr) {
            sDummyHorses.erase(client.clientId);
            return;
        }
        sDummyHorses[client.clientId] = horse;
    }
    s32 ganon = MergeShipAnchor_DummyIsGanondorf(&player->actor) != 0;
    EnHorse_MergeShipPlacePuppet(horse, play, &player->actor.world.pos, player->actor.shape.rot.y,
                                 !ganon && (client.linkAge == LINK_AGE_CHILD), ganon);
}

void DummyPlayer_Init(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    uint32_t clientId = Anchor::Instance->GetDummyPlayerClientId(actor);

    if (!Anchor::Instance->clients.contains(clientId)) {
        Actor_Kill(actor);
        return;
    }

    AnchorClient& client = Anchor::Instance->clients[clientId];

    // Hack to account for usage of gSaveContext in Player_Init
    s32 originalAge = gSaveContext.linkAge;
    gSaveContext.linkAge = client.linkAge;

    // #region modeled after EnTorch2_Init and Player_Init
    actor->room = -1;
    player->itemAction = player->heldItemAction = -1;
    player->heldItemId = ITEM_NONE;
    Player_UseItem(play, player, ITEM_NONE);
    Player_SetModelGroup(player, Player_ActionToModelGroup(player, player->heldItemAction));
    play->playerInit(player, play, gPlayerSkelHeaders[client.linkAge]);

    // Prevent dummy players from holding a weapon trail effect slot, as they don't use it anyway
    Effect_Delete(play, player->meleeWeaponEffectIndex);
    player->meleeWeaponEffectIndex = TOTAL_EFFECT_COUNT;

    play->func_11D54(player, play);
    // #endregion

    player->cylinder.base.acFlags = AC_ON | AC_TYPE_PLAYER;
    player->cylinder.base.ocFlags2 = OC2_TYPE_1;
    player->cylinder.info.bumperFlags = BUMP_ON | BUMP_HOOKABLE | BUMP_NO_HITMARK;
    player->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    player->cylinder.dim.radius = 30;
    player->actor.colChkInfo.damageTable = &DummyPlayerDamageTable;

    gSaveContext.linkAge = originalAge;

    // MergeShip2: a remote player in a transformation form is rebuilt as that form.
    MergeShipAnchor_DummyInit(player, play, clientId, client.linkAge);

    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    if (!isGlobalRoom) {
        NameTag_RegisterForActorWithOptions(actor, client.name.c_str(), {});
    }
}

void Math_Vec3s_Copy(Vec3s* dest, Vec3s* src) {
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
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

    if (client.sceneNum != gPlayState->sceneNum || !client.online || !client.isSaveLoaded) {
        actor->world.pos.x = -9999.0f;
        actor->world.pos.y = -9999.0f;
        actor->world.pos.z = -9999.0f;
        actor->shape.shadowAlpha = 0;
        DummyPlayer_RemoveHorse(play, clientId); // MergeShip2 (BUG-0136)
        return;
    }

    actor->shape.shadowAlpha = 255;
    // MergeShip2: the remote player's form (rebuilding the skeleton first), size and the forms' drawing state.
    MergeShipAnchor_DummyUpdate(player, play, clientId, client.linkAge);
    Math_Vec3s_Copy(&player->upperLimbRot, &client.upperLimbRot);
    Math_Vec3s_Copy(&actor->shape.rot, &client.posRot.rot);
    Math_Vec3f_Copy(&actor->world.pos, &client.posRot.pos);
    player->skelAnime.jointTable = client.jointTable;
    player->skelAnime.movementFlags = client.movementFlags;
    Math_Vec3s_Copy(&player->skelAnime.prevTransl, &client.prevTransl);
    player->currentBoots = client.currentBoots;
    player->currentShield = client.currentShield;
    player->heldItemId = client.buttonItem0;
    player->currentTunic = client.currentTunic;
    player->stateFlags1 = client.stateFlags1;
    player->stateFlags2 = client.stateFlags2;
    player->itemAction = client.itemAction;
    player->heldItemAction = client.heldItemAction;
    player->invincibilityTimer = client.invincibilityTimer;
    player->unk_862 =
        (client.unk_862 > (s16)GID_MAXIMUM) ? (s16)GID_STONE_OF_AGONY : client.unk_862; // prevent OOB, show SoA if OOB
    player->unk_85C = client.unk_85C;
    player->av1.actionVar1 = client.actionVar1;

    // Apply animation movement (Copied from Player_ApplyAnimMovementScaledByAge)
    Vec3f diff;
    SkelAnime_UpdateTranslation(&player->skelAnime, &diff, player->actor.shape.rot.y);

    if (player->skelAnime.movementFlags & 1) {
        // MergeShip2: the dummy's own age or form, not the local player's LINK_IS_ADULT.
        f32 rootScaleXZ = MergeShipAnchor_DummyRootScaleXZ(player, client.linkAge);
        diff.x *= rootScaleXZ;
        diff.z *= rootScaleXZ;

        player->actor.world.pos.x += diff.x * player->actor.scale.x;
        player->actor.world.pos.z += diff.z * player->actor.scale.z;
    }

    if (player->skelAnime.movementFlags & 2) {
        if (!(player->skelAnime.movementFlags & 4)) {
            diff.y *= player->ageProperties->unk_08;
        }

        player->actor.world.pos.y += diff.y * player->actor.scale.y;
    }

    // MergeShip2 (BUG-0136): their horse, if they ride
    DummyPlayer_UpdateHorse(player, play, client);

    if (player->modelGroup != client.modelGroup) {
        // Hack to account for usage of gSaveContext
        s32 originalAge = gSaveContext.linkAge;
        gSaveContext.linkAge = client.linkAge;
        u8 originalButtonItem0 = gSaveContext.equips.buttonItems[0];
        gSaveContext.equips.buttonItems[0] = client.buttonItem0;
        Player_SetModelGroup(player, client.modelGroup);
        gSaveContext.linkAge = originalAge;
        gSaveContext.equips.buttonItems[0] = originalButtonItem0;
    }

    if (Anchor::Instance->roomState.pvpMode == 0 ||
        (Anchor::Instance->roomState.pvpMode == 1 &&
         client.teamId == CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default"))) {
        actor->flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
        return;
    }

    actor->flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;

    if (player->cylinder.base.acFlags & AC_HIT && player->invincibilityTimer == 0) {
        Anchor::Instance->SendPacket_DamagePlayer(client.clientId, player->actor.colChkInfo.damageEffect,
                                                  player->actor.colChkInfo.damage);
        if (player->actor.colChkInfo.damageEffect == DUMMY_PLAYER_HIT_RESPONSE_STUN) {
            Actor_SetColorFilter(&player->actor, 0, 0xFF, 0, 24);
        } else {
            player->invincibilityTimer = 20;
        }
    }

    Collider_UpdateCylinder(&player->actor, &player->cylinder);

    if (!(player->stateFlags2 & PLAYER_STATE2_FROZEN)) {
        if (!(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_HANGING_OFF_LEDGE |
                                     PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_ON_HORSE))) {
            CollisionCheck_SetOC(play, &play->colChkCtx, &player->cylinder.base);
        }

        if (!(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_DAMAGED)) &&
            (player->invincibilityTimer <= 0)) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &player->cylinder.base);

            if (player->invincibilityTimer < 0) {
                CollisionCheck_SetAT(play, &play->colChkCtx, &player->cylinder.base);
            }
        }
    }

    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE)) {
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

    if (client.sceneNum != gPlayState->sceneNum || !client.online || !client.isSaveLoaded) {
        return;
    }

    // Hack to account for usage of gSaveContext in Player_Draw
    s32 originalAge = gSaveContext.linkAge;
    gSaveContext.linkAge = client.linkAge;
    u8 originalButtonItem0 = gSaveContext.equips.buttonItems[0];
    gSaveContext.equips.buttonItems[0] = client.buttonItem0;

    // MergeShip2: the remote player's colours and MM state for its form's drawing.
    if (MergeShipAnchor_DummyBeginDraw(player, play, clientId)) {
        Player_Draw((Actor*)player, play);
    }
    MergeShipAnchor_DummyEndDraw(player, play);
    gSaveContext.linkAge = originalAge;
    gSaveContext.equips.buttonItems[0] = originalButtonItem0;
}

void DummyPlayer_Destroy(Actor* actor, PlayState* play) {
    // MergeShip2 (BUG-0136): their horse goes with them (if it is still here: a scene's end frees it first)
    if (Anchor::Instance != nullptr) {
        DummyPlayer_RemoveHorse(play, Anchor::Instance->GetDummyPlayerClientId(actor));
    }
    // DummyPlayer Actors are initially spawned as ACTOR_PLAYER, but change their
    // ID shortly afterwards to ACTOR_EN_OE2. This would cause ACTOR_PLAYER's
    // ActorDB Entry's `numLoaded` to leak, which is mostly harmless but hits debug
    // asserts. Set the id back to ACTOR_PLAYER so that `numLoaded` will be decremented
    // correctly.
    actor->id = ACTOR_PLAYER;
}
