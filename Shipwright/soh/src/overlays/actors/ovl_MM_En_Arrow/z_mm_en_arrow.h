#ifndef Z_MM_EN_ARROW_H
#define Z_MM_EN_ARROW_H

// MergeShip2 -- Majora's Mask's En_Arrow (2ship2harkinian/mm/src/overlays/actors/ovl_En_Arrow), ported for
// the Deku's projectiles: the magic bubble (B) and the Deku nut dropped while flying. Registered at run
// time (see soh/MergeShip/forms/MergeShipDekuActors.cpp); spawn it with gMergeShipActorId_MM_En_Arrow.
// Ocarina of Time's own bow, slingshot and nut throw keep using Ocarina of Time's En_Arrow.

#include <libultraship/libultra.h>
#include "global.h"

struct MMEnArrow;

typedef void (*MMEnArrowActionFunc)(struct MMEnArrow*, PlayState*);

// MM ArrowType, MM's values.
typedef enum MMArrowType {
    /* 0 */ MM_ARROW_TYPE_NORMAL_LIT,
    /* 1 */ MM_ARROW_TYPE_NORMAL_HORSE,
    /* 2 */ MM_ARROW_TYPE_NORMAL,
    /* 3 */ MM_ARROW_TYPE_FIRE,
    /* 4 */ MM_ARROW_TYPE_ICE,
    /* 5 */ MM_ARROW_TYPE_LIGHT,
    /* 6 */ MM_ARROW_TYPE_SLINGSHOT,
    /* 7 */ MM_ARROW_TYPE_DEKU_BUBBLE,
    /* 8 */ MM_ARROW_TYPE_DEKU_NUT
} MMArrowType;

// MM ARROW_MAGIC_DEKU_BUBBLE's cost (MM sMagicArrowCosts).
#define MM_ARROW_MAGIC_DEKU_BUBBLE_COST 2

// MM EnArrowBubble.
typedef struct MMEnArrowBubble {
    /* 0x144 */ f32 unk_144; // size
    /* 0x148 */ u8 unk_148;
    /* 0x149 */ s8 unk_149;
    /* 0x14A */ s16 unk_14A;
    /* 0x14C */ s16 unk_14C;
} MMEnArrowBubble;

typedef struct MMEnArrow {
    /* 0x000 */ Actor actor;
    /* 0x144 */ MMEnArrowBubble bubble;
    /* 0x1A8 */ ColliderQuad collider;
    /* 0x228 */ Vec3f unk_228;
    /* 0x234 */ Vec3f unk_234;
    /* 0x244 */ WeaponInfo unk_244;
    /* 0x260 */ u8 unk_260; // timer
    /* 0x261 */ u8 unk_261; // hit flags
    /* 0x262 */ u8 unk_262; // touched a poly
    /* 0x263 */ u8 unk_263; // cutscene nut
    /* 0x264 */ Actor* unk_264;
    /* 0x268 */ Vec3f unk_268;
    /* 0x274 */ MMEnArrowActionFunc actionFunc;
} MMEnArrow;

#ifdef __cplusplus
extern "C" {
#endif

extern s16 gMergeShipActorId_MM_En_Arrow;

void MMEnArrow_Init(Actor* thisx, PlayState* play);
void MMEnArrow_Destroy(Actor* thisx, PlayState* play);
void MMEnArrow_Update(Actor* thisx, PlayState* play);
void MMEnArrow_Draw(Actor* thisx, PlayState* play);

#ifdef __cplusplus
}
#endif

#endif
