#ifndef Z_OOT_MAGIC_WIND_H
#define Z_OOT_MAGIC_WIND_H

#include "global.h"
#include "z64curve.h"

struct OoTMagicWind;

typedef void (*OoTMagicWindFunc)(struct OoTMagicWind* this, PlayState* play);

// MergeShip2: Ocarina of Time's Magic_Wind (Farore's Wind), ported to Majora's Mask.
// Same layout as OoT's MagicWind (soh/src/overlays/actors/ovl_Magic_Wind/z_magic_wind.h); OoT's SkelAnimeCurve is
// MM's SkelCurve (the same struct under MM's field names).
typedef struct OoTMagicWind {
    /* 0x0000 */ Actor actor;
    /* 0x0144 */ SkelCurve skelCurve;
    /* 0x0164 */ s16 timer;
    /* 0x0168 */ OoTMagicWindFunc actionFunc;
} OoTMagicWind;

#endif
