#ifndef Z_EN_DOG_H
#define Z_EN_DOG_H

#include <libultraship/libultra.h>
#include "global.h"

struct EnDog;

typedef void (*EnDogActionFunc)(struct EnDog*, PlayState*);

typedef struct EnDog {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ SkelAnime skelAnime;
    /* 0x0190 */ EnDogActionFunc actionFunc;
    /* 0x0194 */ ColliderCylinder collider;
    /* 0x01E0 */ Path* path;
    /* 0x01E4 */ u8 reverse;
    /* 0x01E6 */ s16 waypoint;
    /* 0x01E8 */ s16 unusedAngle;
    /* 0x01EA */ s16 pad1;
    /* 0x01EC */ s16 behaviorTimer;
    /* 0x01EE */ s16 pad2;
    /* 0x01F0 */ s16 nextBehavior; 
    /* 0x01F2 */ s16 behavior;
    /* 0x01F4 */ Vec3s jointTable[13];
    /* 0x0242 */ Vec3s morphTable[13];
    // MergeShip2: MM EnDg's Bremen Mask follower state (z_en_dg.c).
    u16 bremenFollowing;                   // MM DOG_FLAG_FOLLOWING_BREMEN_MASK
    s16 bremenBarkTimer;                   // MM EnDg::bremenBarkTimer
    EnDogActionFunc bremenPrevActionFunc;  // what the dog was doing when the march took it
    // MergeShip2 (soh_fd): MM EnDg's reactions to the Deku, Goron and Zora (z_en_dg.c).
    s16 mmBehavior;                    // MM EnDg::behavior (DOG_BEHAVIOR_*)
    s16 mmTimer;                       // MM EnDg::timer
    s16 mmAttackTimer;                 // MM EnDg::attackTimer
    u16 mmDogFlags;                    // MM EnDg::dogFlags (DOG_FLAG_JUMP_ATTACKING, DOG_FLAG_BOUNCED)
    EnDogActionFunc mmPrevActionFunc;  // what the dog was doing when a form took it (NULL while it is not taken)
} EnDog; // size = 0x0290

#endif
