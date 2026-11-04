#ifndef Z_MM_EN_ELFORG_H
#define Z_MM_EN_ELFORG_H

// MergeShip2 -- Majora's Mask's Stray Fairy (2ship2harkinian/mm/src/overlays/actors/ovl_En_Elforg), ported.
// Registered at run time (soh/MergeShip/MergeShipStrayFairy.cpp) as gMergeShipActorId_MM_En_Elforg; spawn it with
// MM_STRAY_FAIRY_PARAMS(flag, area, type), or through MergeShipStrayFairy.h's API.

#include <libultraship/libultra.h>
#include "global.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipStrayFairy.h"

// MM z_en_elforg.h
#define MM_STRAY_FAIRY_TYPE(thisx) ((thisx)->params & 0xF)
#define MM_STRAY_FAIRY_GET_NON_DUNGEON_AREA(thisx) (((thisx)->params & 0x1C0) >> 6)
#define MM_STRAY_FAIRY_GET_FLAG(thisx) (((thisx)->params & 0xFE00) >> 9)
#define MM_STRAY_FAIRY_SPARKLE_COUNT(thisx) ((thisx)->home.rot.x)

#define MM_STRAY_FAIRY_FLAG_MOVES_QUICKLY_TO_HOME (1 << 0)
#define MM_STRAY_FAIRY_FLAG_SPARKLES_AND_SHRINKS (1 << 1)
#define MM_STRAY_FAIRY_FLAG_CIRCLES_QUICKLY_IN_FOUNTAIN (1 << 2)
#define MM_STRAY_FAIRY_FLAG_GREAT_FAIRYS_MASK_EQUIPPED (1 << 3)

struct MMEnElforg;

typedef void (*MMEnElforgActionFunc)(struct MMEnElforg*, PlayState*);

typedef struct MMEnElforg {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[STRAY_FAIRY_LIMB_MAX];
    ColliderCylinder collider;
    Actor* enemy;
    u16 strayFairyFlags;
    s16 direction; // negative when facing right, positive when facing left
    s16 area;
    s32 timer;
    s32 secondaryTimer;
    f32 targetSpeedXZ;
    f32 targetDistanceFromHome;
    MMEnElforgActionFunc actionFunc;
    // MergeShip: handed to the collect override (MergeShip_StrayFairy_SetCollectOverride), e.g. a randomizer check.
    u32 userData;
} MMEnElforg;

#ifdef __cplusplus
extern "C" {
#endif

void MMEnElforg_Init(Actor* thisx, PlayState* play);
void MMEnElforg_Destroy(Actor* thisx, PlayState* play);
void MMEnElforg_Update(Actor* thisx, PlayState* play);
void MMEnElforg_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif

#endif
