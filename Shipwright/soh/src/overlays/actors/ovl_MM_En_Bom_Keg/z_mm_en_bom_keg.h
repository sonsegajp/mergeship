#ifndef Z_MM_EN_BOM_KEG_H
#define Z_MM_EN_BOM_KEG_H

// MergeShip2 -- Majora's Mask's Powder Keg: the `isPowderKeg` branch of MM En_Bom
// (2ship2harkinian/mm/src/overlays/actors/ovl_En_Bom), as an actor of its own. Ocarina of Time keeps its own
// En_Bom (ACTOR_EN_BOM) for bombs; this one is registered at run time (MergeShipActorMMEnBomKeg.cpp) and spawned by
// the player with gMergeShipActorId_MM_En_Bom_Keg, as MM's Player_InitExplosiveIA spawns En_Bom:
//
//   Actor_SpawnAsChild(&play->actorCtx, &player->actor, play, gMergeShipActorId_MM_En_Bom_Keg,
//                      pos.x, pos.y, pos.z, 0, player->actor.shape.rot.y, 0, MMENBOMKEG_BODY);
//
// with gMergeShipPowderKegTimer already set (MM sets gSaveContext.powderKegTimer = 200 first when it is 0). The
// player then holds it as heldActor (PLAYER_STATE1_CARRYING_ACTOR) and throws / puts it down through Ocarina of
// Time's carried-actor code (which clears `parent`), exactly as it does a bomb.

#include <libultraship/libultra.h>
#include "global.h"

struct MMEnBomKeg;

typedef void (*MMEnBomKegActionFunc)(struct MMEnBomKeg*, PlayState*);

// MM BombType, in `params`. The values are Ocarina of Time's BOMB_BODY / BOMB_EXPLOSION too, which Ocarina of Time's
// actors test on every ACTORCAT_EXPLOSIVE actor (func_80033684, Obj_Bombiwa, En_Kanban, Boss_Ganon ...).
typedef enum {
    /* 0 */ MMENBOMKEG_BODY,
    /* 1 */ MMENBOMKEG_EXPLOSION
} MMEnBomKegType;

// MM EnBom, with MM's field order: the colliders and `timer` sit where Ocarina of Time's EnBom and EnBombf have
// theirs, because Ocarina of Time's actors cast any explosive they find (En_Dodongo holds an eaten one with
// `((EnBombf*)actor)->timer++`). Checked by static asserts in the .c.
typedef struct MMEnBomKeg {
    /*       */ Actor actor;
    /*       */ ColliderCylinder collider1;               // the keg's body (AC / OC)
    /*       */ ColliderJntSph collider2;                 // the explosion (AT)
    /*       */ ColliderJntSphElement collider2Elements[1];
    /*       */ s16 timer;
    /*       */ s16 flashSpeedScale;
    /*       */ f32 unk_1F4;
    /*       */ u8 unk_1F8;       // bump (OC) enabled once the player is away from it
    /*       */ u8 isPowderKeg;   // always true: kept so the port reads like MM
    /*       */ s16 unk_1FA;      // roll angle
    /*       */ u8 unk_1FC;       // sinking countdown after landing in deep water
    /*       */ u8 parentChecked; // MergeShip2: MM Init's Actor_HasParent test, deferred to the first update (see .c)
    /*       */ MMEnBomKegActionFunc actionFunc;
} MMEnBomKeg;

#ifdef __cplusplus
extern "C" {
#endif

void MMEnBomKeg_Init(Actor* thisx, PlayState* play);
void MMEnBomKeg_Destroy(Actor* thisx, PlayState* play);
void MMEnBomKeg_Update(Actor* thisx, PlayState* play);
void MMEnBomKeg_Draw(Actor* thisx, PlayState* play);

// Run-time actor id assigned by ActorDB (MergeShipActorMMEnBomKeg.cpp); -1 until registered.
extern s16 gMergeShipActorId_MM_En_Bom_Keg;

// MM `play->actorCtx.flags & ACTORCTX_FLAG_0`: nonzero while a powder keg actor exists (set in Init, cleared in
// Destroy). MM's Goron keg seller (En_Go) and its message script refuse to sell a keg while one exists; Ocarina of
// Time's ActorContext has no such flag, so it lives here for whichever port needs it.
extern u8 gMergeShipPowderKegActorExists;

#ifdef __cplusplus
}
#endif

#endif // Z_MM_EN_BOM_KEG_H
