#ifndef Z_MM_EN_BOOM_H
#define Z_MM_EN_BOOM_H

// MergeShip2 -- Majora's Mask's En_Boom (the Zora's fin boomerangs), ported from
// 2ship2harkinian/mm/src/overlays/actors/ovl_En_Boom. Ocarina of Time's own En_Boom (the boomerang
// item) is a different actor and is left alone; this one is registered with ActorDB at run time
// (see soh/MergeShip/forms/MergeShipZoraActors.cpp) and spawned by gMergeShipActorId_EnBoom.

#include "global.h"

// MM EnBoomType.
typedef enum MMEnBoomType {
    /* 0 */ MM_ZORA_BOOMERANG_LEFT,
    /* 1 */ MM_ZORA_BOOMERANG_RIGHT
} MMEnBoomType;

struct MMEnBoom;

typedef void (*MMEnBoomActionFunc)(struct MMEnBoom*, PlayState*);

// MM EnBoom, same fields and meanings.
typedef struct MMEnBoom {
    /* 0x000 */ Actor actor;
    /* 0x144 */ ColliderQuad collider;
    /* 0x1C4 */ Actor* moveTo;
    /* 0x1C8 */ Actor* unk_1C8; // grabbed collectible / Gold Skulltula token
    /* 0x1CC */ u8 unk_1CC;     // frames until it turns back
    /* 0x1CD */ u8 unk_1CD;     // spin
    /* 0x1CE */ u8 unk_1CE;     // frames of the initial outward curve on the way back
    /* 0x1CF */ s8 unk_1CF;     // frames of tight homing on a locked-on target
    /* 0x1D0 */ s32 effectIndex;
    /* 0x1D4 */ WeaponInfo weaponInfo;
    /* 0x1F0 */ MMEnBoomActionFunc actionFunc;
} MMEnBoom;

#ifdef __cplusplus
extern "C" {
#endif

void MMEnBoom_Init(Actor* thisx, PlayState* play);
void MMEnBoom_Destroy(Actor* thisx, PlayState* play);
void MMEnBoom_Update(Actor* thisx, PlayState* play);
void MMEnBoom_Draw(Actor* thisx, PlayState* play);

// The actor id ActorDB assigned at boot (-1 until registered).
extern s16 gMergeShipActorId_EnBoom;

#ifdef __cplusplus
}
#endif

#endif
