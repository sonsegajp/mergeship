#ifndef Z_MM_OBJ_ETCETERA_H
#define Z_MM_OBJ_ETCETERA_H

// MergeShip2 -- Majora's Mask's Deku Flower (2ship2harkinian/mm/src/overlays/actors/ovl_Obj_Etcetera), ported.
// Registered at run time (soh/MergeShip/forms/MergeShipDekuActors.cpp); spawn it with
// gMergeShipActorId_MM_Obj_Etcetera and params MM_DEKU_FLOWER_PARAMS(type).

#include <libultraship/libultra.h>
#include "global.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"

#define MM_DEKU_FLOWER_TYPE(thisx) (((thisx)->params & 0xFF80) >> 7)
#define MM_DEKU_FLOWER_PARAMS(type) (((type) << 7) & 0xFF80)

typedef enum MMDekuFlowerType {
    /* 0 */ MM_DEKU_FLOWER_TYPE_PINK,
    /* 1 */ MM_DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE,
    /* 2 */ MM_DEKU_FLOWER_TYPE_GOLD,
    /* 3 */ MM_DEKU_FLOWER_TYPE_GOLD_WITH_INITIAL_BOUNCE,
    /* 4 */ MM_DEKU_FLOWER_TYPE_MAX
} MMDekuFlowerType;

struct MMObjEtcetera;

typedef void (*MMObjEtceteraActionFunc)(struct MMObjEtcetera*, PlayState*);

typedef struct MMObjEtcetera {
    /* 0x000 */ DynaPolyActor dyna;
    /* 0x15C */ SkelAnime skelAnime;
    /* 0x1A0 */ ColliderCylinder collider;
    /* 0x1EC */ Vec3s jointTable[PINK_DEKU_FLOWER_LIMB_MAX];
    /* 0x22E */ Vec3s morphTable[PINK_DEKU_FLOWER_LIMB_MAX];
    /* 0x270 */ f32 bounceOscillationScale;
    /* 0x274 */ s16 oscillationTimer;
    /* 0x276 */ u16 burrowFlag;
    /* 0x278 */ s8 objectSlot;
    /* 0x27C */ Gfx* dList;
    /* 0x280 */ MMObjEtceteraActionFunc actionFunc;
} MMObjEtcetera; // size = 0x284

#ifdef __cplusplus
extern "C" {
#endif

extern s16 gMergeShipActorId_MM_Obj_Etcetera;

void MMObjEtcetera_Init(Actor* thisx, PlayState* play);
void MMObjEtcetera_Destroy(Actor* thisx, PlayState* play);
void MMObjEtcetera_Update(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif

#endif
