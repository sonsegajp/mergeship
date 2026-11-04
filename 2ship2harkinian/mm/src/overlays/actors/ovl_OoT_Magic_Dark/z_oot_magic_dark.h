#ifndef Z_OOT_MAGIC_DARK_H
#define Z_OOT_MAGIC_DARK_H

#include "global.h"

// MergeShip2: Ocarina of Time's Magic_Dark (Nayru's Love), ported to Majora's Mask.
// Same layout as OoT's MagicDark (soh/src/overlays/actors/ovl_Magic_Dark/z_magic_dark.h).
typedef struct OoTMagicDark {
    /* 0x0000 */ Actor actor;
    /* 0x0144 */ s16 timer;
    /* 0x0146 */ u8 primAlpha;
    /* 0x0148 */ Vec3f orbOffset;
    /* 0x0154 */ f32 scale;
    /* 0x0158 */ char unk_158[0x4];
} OoTMagicDark;

#endif
