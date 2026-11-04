#ifndef Z_MM_OBJ_AQUA_H
#define Z_MM_OBJ_AQUA_H

// MergeShip2 -- Majora's Mask's Obj_Aqua (the water poured out of a bottle), ported from
// 2ship2harkinian/mm/src/overlays/actors/ovl_Obj_Aqua. Ocarina of Time has no such actor; this one is
// registered at run time (soh/MergeShip/MergeShipActorMMObjAqua.cpp) and spawned by the player, where MM's
// Player_Action_70 spawns it, with gMergeShipActorId_MM_Obj_Aqua and params MM_AQUA_PARAMS(type).
// Host-game differences are listed at the top of z_mm_obj_aqua.c.

#include <libultraship/libultra.h>
#include "global.h"

// MM AQUA_GET_TYPE / AQUA_PARAMS
#define MM_AQUA_GET_TYPE(thisx) ((thisx)->params & 1)
#define MM_AQUA_PARAMS(type) ((type) & 1)

// MM AquaType
enum {
    /* 0 */ MM_AQUA_TYPE_COLD = 0,
    /* 1 */ MM_AQUA_TYPE_HOT = 1
};

struct MMObjAqua;

typedef void (*MMObjAquaActionFunc)(struct MMObjAqua*, PlayState*);

typedef struct MMObjAqua {
    /* 0x000 */ Actor actor;
    /* 0x144 */ ColliderCylinder collider;
    /* 0x190 */ MMObjAquaActionFunc actionFunc;
    /* 0x194 */ s16 counter;
    /* 0x196 */ u8 alpha;
    /* 0x198 */ s16 unk_198;
} MMObjAqua; // size = 0x19C (MM)

#ifdef __cplusplus
extern "C" {
#endif

// Run-time actor id assigned by ActorDB (MergeShipActorMMObjAqua.cpp); -1 until registered.
extern s16 gMergeShipActorId_MM_Obj_Aqua;

void MMObjAqua_Init(Actor* thisx, PlayState* play);
void MMObjAqua_Destroy(Actor* thisx, PlayState* play);
void MMObjAqua_Update(Actor* thisx, PlayState* play);
void MMObjAqua_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif

#endif // Z_MM_OBJ_AQUA_H
