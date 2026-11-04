#ifndef Z_OOT_MAGIC_FIRE_H
#define Z_OOT_MAGIC_FIRE_H

#include "global.h"

// MergeShip2: Ocarina of Time's Magic_Fire (Din's Fire), ported to Majora's Mask.
// Same layout as OoT's MagicFire (soh/src/overlays/actors/ovl_Magic_Fire/z_magic_fire.h).
typedef struct OoTMagicFire {
    /* 0x0000 */ Actor actor;
    /* 0x0144 */ ColliderCylinder collider;
    /* 0x0190 */ f32 alphaMultiplier;
    /* 0x0194 */ f32 screenTintIntensity;
    /* 0x0198 */ f32 scalingSpeed;
    /* 0x019C */ s16 action;
    /* 0x019E */ s16 screenTintBehaviour;
    /* 0x01A0 */ s16 actionTimer;
    /* 0x01A2 */ s16 screenTintBehaviourTimer;
} OoTMagicFire;

#endif
