#ifndef Z_MM_EN_M_THUNDER_H
#define Z_MM_EN_M_THUNDER_H

// MergeShip2 -- Majora's Mask's En_M_Thunder (spin attack wave and sword beam), ported from
// 2ship2harkinian/mm/src/overlays/actors/ovl_En_M_Thunder. Ocarina of Time keeps its own En_M_Thunder;
// this one is registered at run time (MergeShipActorMMEnMThunder.cpp) and spawned by the player with
// gMergeShipActorId_En_M_Thunder.

#include <libultraship/libultra.h>
#include "global.h"

struct MMEnMThunder;

#define MMENMTHUNDER_GET_TYPE(thisx) ((thisx)->params & 0xFF)
#define MMENMTHUNDER_GET_MAGIC_COST(thisx) (((thisx)->params & 0xFF00) >> 8)

// MM EnMThunderType: `heldItemAction - PLAYER_IA_SWORD_KOKIRI` in MM's item-action order.
typedef enum {
    /* 0x00 */ MMENMTHUNDER_TYPE_KOKIRI_SWORD,
    /* 0x01 */ MMENMTHUNDER_TYPE_RAZOR_SWORD,
    /* 0x02 */ MMENMTHUNDER_TYPE_GILDED_SWORD,
    /* 0x03 */ MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD, // also Fierce Deity's sword (MM PLAYER_IA_SWORD_TWO_HANDED)
    /* 0x80 */ MMENMTHUNDER_TYPE_UNK = 0x80
} MMEnMThunderType;

typedef void (*MMEnMThunderActionFunc)(struct MMEnMThunder*, PlayState*);

typedef struct MMEnMThunder {
    /* 0x000 */ Actor actor;
    /* 0x144 */ ColliderCylinder collider;
    /* 0x190 */ LightNode* lightNode;
    /* 0x194 */ LightInfo lightInfo;
    /* 0x1A4 */ f32 lightColorFrac;
    /* 0x1A8 */ f32 alphaFrac;
    /* 0x1AC */ f32 scroll;
    /* 0x1B0 */ f32 unk1B0;
    /* 0x1B4 */ f32 adjustLightsArg1;
    /* 0x1B8 */ MMEnMThunderActionFunc actionFunc;
    /* 0x1BC */ u16 timer;
    /* 0x1BE */ u8 subtype;
    /* 0x1BF */ u8 type;
    /* 0x1C0 */ u8 chargingAlpha;
    /* 0x1C1 */ u8 scaleTarget;
    /* 0x1C2 */ u8 isCharging;
} MMEnMThunder; // size = 0x1C4

#ifdef __cplusplus
extern "C" {
#endif

void MMEnMThunder_Init(Actor* thisx, PlayState* play);
void MMEnMThunder_Destroy(Actor* thisx, PlayState* play);
void MMEnMThunder_Update(Actor* thisx, PlayState* play);
void MMEnMThunder_Draw(Actor* thisx, PlayState* play);

// Run-time actor id assigned by ActorDB (MergeShipActorMMEnMThunder.cpp); -1 until registered.
extern s16 gMergeShipActorId_En_M_Thunder;

// True for a Fierce Deity sword beam (as opposed to a spin-attack wave).
s32 MMEnMThunder_IsSwordBeam(Actor* actor);

#ifdef __cplusplus
}
#endif

#endif // Z_MM_EN_M_THUNDER_H
