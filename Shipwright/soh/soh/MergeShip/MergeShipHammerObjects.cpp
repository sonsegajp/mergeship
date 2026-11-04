// MergeShip2 -- the Goron works Ocarina of Time's Megaton Hammer objects (see MergeShipHammerObjects.h).
//
// Ocarina of Time's hammer objects and how each reacts to the Goron:
//   Obj_Switch (rusted floor switch)  hammer-only, face up   collider (pound) + MergeShip_GoronHammer_Strikes
//   Bg_Hidan_Hamstep (hammer step)    hammer-only, face up   collider (pound) + MergeShip_GoronHammer_Strikes
//   Bg_Hidan_Hrock (sinking pillars)  hammer-only, face up   collider (pound) + MergeShip_GoronHammer_Strikes
//   Bg_Hidan_Dalm (totems)            hammer swing only      VB_HAMMER_TOTEM_BREAK hook below (punch)
//   Obj_Hamishi (bronze boulder)      hammer-only            collider: the punch and the pound already hit it
//   En_Wonder_Item (hammer tag)       hammer-only            collider
//   Obj_Bombiwa, Obj_Kibako2, Door_Ana (hidden grotto), En_Ishi (small rock)
//                                     hammer or explosive    collider
// Bomb walls (Bg_Bombwall, Bg_Breakwall, ...) belong to "MergeShip.GoronBreaksBombWalls" (MergeShipBombWalls.h).

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

#include <cmath>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipHammerObjects.h"
#include "src/overlays/actors/ovl_Bg_Hidan_Dalm/z_bg_hidan_dalm.h"
#include "src/overlays/actors/ovl_Obj_Switch/z_obj_switch.h"

extern PlayState* gPlayState;
// MergeShipGoron.h (C only: its prototypes name parameters `this`).
void MergeShip_Goron_Action_96(Player* player, PlayState* play);
void MergeShipTest_Log(const char* fmt, ...);
int MergeShipTest_IsActive(void);
}

#define CVAR_GORON_HAMMER_NAME CVAR_ENHANCEMENT("MergeShip.GoronHammerObjects")
#define CVAR_GORON_HAMMER_VALUE CVarGetInteger(CVAR_GORON_HAMMER_NAME, 1)

namespace {

enum HammerObjectKind {
    HAMMER_OBJECT_NONE,
    HAMMER_OBJECT_HAMMER_ONLY,     // only the Megaton Hammer works it
    HAMMER_OBJECT_HAMMER_OR_BOMB,  // the hammer or an explosive
};

HammerObjectKind GetHammerObjectKind(Actor* actor) {
    switch (actor->id) {
        case ACTOR_OBJ_SWITCH:
            return ((actor->params & 7) == OBJSWITCH_TYPE_FLOOR_RUSTY) ? HAMMER_OBJECT_HAMMER_ONLY : HAMMER_OBJECT_NONE;
        case ACTOR_BG_HIDAN_HAMSTEP:
        case ACTOR_BG_HIDAN_HROCK:
        case ACTOR_BG_HIDAN_DALM:
        case ACTOR_OBJ_HAMISHI:
        case ACTOR_EN_WONDER_ITEM:
            return HAMMER_OBJECT_HAMMER_ONLY;
        case ACTOR_OBJ_BOMBIWA:
        case ACTOR_OBJ_KIBAKO2:
        case ACTOR_DOOR_ANA:
        case ACTOR_EN_ISHI:
            return HAMMER_OBJECT_HAMMER_OR_BOMB;
        default:
            return HAMMER_OBJECT_NONE;
    }
}

f32 DistXZ(const Vec3f* a, const Vec3f* b) {
    f32 dx = a->x - b->x;
    f32 dz = a->z - b->z;

    return sqrtf(SQ(dx) + SQ(dz));
}

// The Goron's punch is out (a Goron has no sword: meleeWeaponState is its fists').
bool GoronPunching(Player* player) {
    return player->meleeWeaponState != 0;
}

// The Goron's ground pound has landed (MM Player_Action_96, actionVar1 2: the pound's attack cylinder is out).
bool GoronPoundLanded(Player* player) {
    return (player->actionFunc == MergeShip_Goron_Action_96) && (player->av1.actionVar1 == 2);
}

// One of the Goron's fists hit a collider element of `collider` last frame.
bool TrisHitByGoronFist(Player* player, ColliderTris* collider) {
    for (s32 i = 0; i < collider->count; i++) {
        ColliderInfo* info = &collider->elements[i].info;

        if ((info->bumperFlags & BUMP_HIT) && ((info->acHit == &player->meleeWeaponQuads[0].base) ||
                                               (info->acHit == &player->meleeWeaponQuads[1].base))) {
            return true;
        }
    }
    return false;
}

} // namespace

extern "C" s32 MergeShip_GoronHammer_Enabled(PlayState* play) {
    Player* player;

    if (play == NULL) {
        return false;
    }
    player = GET_PLAYER(play);
    return (player != NULL) && (player->mergeShipForm == PLAYER_FORM_GORON) && (CVAR_GORON_HAMMER_VALUE != 0);
}

extern "C" s32 MergeShip_GoronHammer_Strikes(PlayState* play, Actor* actor, const Vec3f* top, f32 radius) {
    Player* player;
    const char* how = NULL;

    if (!MergeShip_GoronHammer_Enabled(play)) {
        return false;
    }
    player = GET_PLAYER(play);

    // The punch: the fist reaches over the face, at most a little below it and no higher than a hammer's head
    // would come down from. (Background actors update before the player: this is last frame's punch.)
    if (GoronPunching(player)) {
        const Vec3f* fist = &player->meleeWeaponInfo[0].tip;
        f32 dy = fist->y - top->y;

        if ((dy >= -20.0f) && (dy <= 90.0f) && (DistXZ(fist, top) <= (radius + 20.0f))) {
            how = "punch";
        }
    }
    // The ground pound: landed on the face or beside it (the pound's attack cylinder has a radius of 60).
    if ((how == NULL) && GoronPoundLanded(player)) {
        f32 dy = player->actor.world.pos.y - top->y;

        if ((dy >= -50.0f) && (dy <= 30.0f) && (DistXZ(&player->actor.world.pos, top) <= (radius + 60.0f))) {
            how = "pound";
        }
    }

    if (how == NULL) {
        return false;
    }
    if (MergeShipTest_IsActive()) {
        MergeShipTest_Log("H goron hammer %s works actor 0x%X params=%04X pos=(%.0f,%.0f,%.0f)", how, actor->id,
                          (u16)actor->params, actor->world.pos.x, actor->world.pos.y, actor->world.pos.z);
    }
    return true;
}

extern "C" s32 MergeShip_GoronHammer_SkipHit(Collider* colAT, Collider* colAC) {
    Player* player;
    HammerObjectKind kind;

    if ((colAT->actor == NULL) || (colAT->actor->id != ACTOR_PLAYER) || (colAC->actor == NULL)) {
        return false;
    }
    player = (Player*)colAT->actor;
    if (player->mergeShipForm != PLAYER_FORM_GORON) {
        return false;
    }
    kind = GetHammerObjectKind(colAC->actor);
    if (kind == HAMMER_OBJECT_NONE) {
        return false;
    }
    if (CVAR_GORON_HAMMER_VALUE != 0) {
        return false;
    }
    if (kind == HAMMER_OBJECT_HAMMER_OR_BOMB) {
        return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.GoronBreaksBombWalls"), 1) == 0;
    }
    return true;
}

namespace {

void RegisterGoronHammerObjects() {
    bool enabled = CVAR_GORON_HAMMER_VALUE != 0;

    // Bg_Hidan_Dalm breaks only for Player::meleeWeaponAnimation's hammer swings (22, 23); the Goron's punch holds
    // PLAYER_MWA_FORWARD_SLASH_1H there (MergeShipGoron.h). Its fist hitting the totem's side collider, or its punch
    // striking the totem's collision (the punch rebounds off the wall first), breaks it as the hammer does.
    COND_VB_SHOULD(VB_HAMMER_TOTEM_BREAK, enabled, {
        BgHidanDalm* dalm = va_arg(args, BgHidanDalm*);

        if (!*should && (gPlayState != nullptr) && (dalm != nullptr) && MergeShip_GoronHammer_Enabled(gPlayState) &&
            !Player_InCsMode(gPlayState)) {
            Player* player = GET_PLAYER(gPlayState);

            if ((dalm->collider.base.acFlags & AC_HIT) && TrisHitByGoronFist(player, &dalm->collider)) {
                *should = true;
            } else if (MergeShip_Goron_PunchStruckBg(gPlayState, dalm->dyna.bgId)) {
                // The totem slides away from the struck side: elements 0 and 1 are its +x face (BgHidanDalm_Wait).
                Actor* actor = &dalm->dyna.actor;
                f32 dx = player->actor.world.pos.x - actor->world.pos.x;
                f32 dz = player->actor.world.pos.z - actor->world.pos.z;
                f32 localX = (dx * Math_CosS(actor->shape.rot.y)) - (dz * Math_SinS(actor->shape.rot.y));

                for (s32 i = 0; i < dalm->collider.count; i++) {
                    dalm->collider.elements[i].info.bumperFlags &= ~BUMP_HIT;
                }
                if (localX > 0.0f) {
                    dalm->collider.elements[0].info.bumperFlags |= BUMP_HIT;
                }
                *should = true;
            }

            if (*should && MergeShipTest_IsActive()) {
                MergeShipTest_Log("H goron hammer punch works actor 0x%X params=%04X", dalm->dyna.actor.id,
                                  (u16)dalm->dyna.actor.params);
            }
        }
    });
}

static RegisterShipInitFunc sGoronHammerObjectsInit(RegisterGoronHammerObjects, { CVAR_GORON_HAMMER_NAME });

} // namespace
