#ifndef Z_OOT_EN_BOOM_H
#define Z_OOT_EN_BOOM_H

#include "global.h"

// MergeShip2: Ocarina of Time's En_Boom (the thrown Boomerang), ported to Majora's Mask as its own actor.
// MM's ACTOR_EN_BOOM is the Zora's fins; this is ACTOR_OOT_EN_BOOM.
// Field names are OoT's and are part of the thrower's handshake: right after Actor_Spawn the player sets
// `moveTo` (its lock-on target, may be NULL) and `returnTimer` (20). Keep them stable.

struct OoTEnBoom;

typedef void (*OoTEnBoomActionFunc)(struct OoTEnBoom*, PlayState*);

typedef struct OoTEnBoom {
    /* 0x0000 */ Actor actor;
    /*        */ ColliderQuad collider;
    /*        */ Actor* moveTo;  // actor boomerang moves toward
    /*        */ Actor* grabbed; // actor grabbed by the boomerang
    /*        */ u8 returnTimer; // returns to Link when 0
    /*        */ u8 activeTimer; // increments once every update
    /*        */ s32 effectIndex;
    /*        */ WeaponInfo boomerangInfo;
    /*        */ OoTEnBoomActionFunc actionFunc;
} OoTEnBoom;

#endif
