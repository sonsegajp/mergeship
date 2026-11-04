#ifndef Z_EN_SKB_H
#define Z_EN_SKB_H

#include <libultraship/libultra.h>
#include "global.h"

struct EnSkb;

typedef void (*EnSkbActionFunc)(struct EnSkb*, PlayState*);

typedef struct EnSkb {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ SkelAnime skelAnime;
    /* 0x0190 */ Vec3s jointTable[20];
    /* 0x0208 */ Vec3s morphTable[20];
    /* 0x0280 */ u8 actionState;
    /* 0x0281 */ u8 setColliderAT;
    /* 0x0282 */ u8 lastDamageReaction;
    /* 0x0283 */ u8 breakFlags;
    /* 0x0284 */ EnSkbActionFunc actionFunc; 
    /* 0x0288 */ s16 headlessYawOffset;
    /* 0x028C */ BodyBreak bodyBreak; 
    /* 0x02A4 */ ColliderJntSph collider;
    /* 0x02C4 */ ColliderJntSphElement colliderItem[2];
    // MergeShip2: MM EnSkb Captain's Hat state.
    /*        */ s16 mergeShipTalkedBefore; // MM unk_3E0: 0x13F6 has been said, later talks say 0x13F8
    /*        */ s16 mergeShipTalking;      // MM unk_3E2: in conversation, no collision
    /*        */ u8 mergeShipHintChosen;    // randomizer: mergeShipHint has been picked for this Stalchild
    /*        */ u16 mergeShipHint;         // randomizer: the RandomizerHint this Stalchild reports
} EnSkb; // size = 0x0344

#endif
