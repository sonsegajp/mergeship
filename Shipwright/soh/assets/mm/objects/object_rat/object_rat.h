#ifndef MM_OBJECTS_OBJECT_RAT_H
#define MM_OBJECTS_OBJECT_RAT_H 1

#include "align_asset_macro.h"

#define dmm_gRealBombchuSpotAnim "__OTR__mm/objects/object_rat/gRealBombchuSpotAnim"
static const ALIGN_ASSET(2) char mm_gRealBombchuSpotAnim[] = dmm_gRealBombchuSpotAnim;

#define dmm_gRealBombchuRunAnim "__OTR__mm/objects/object_rat/gRealBombchuRunAnim"
static const ALIGN_ASSET(2) char mm_gRealBombchuRunAnim[] = dmm_gRealBombchuRunAnim;

#define dmm_gRealBombchuBodyDL "__OTR__mm/objects/object_rat/gRealBombchuBodyDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuBodyDL[] = dmm_gRealBombchuBodyDL;

#define dmm_gRealBombchuBackRightLegDL "__OTR__mm/objects/object_rat/gRealBombchuBackRightLegDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuBackRightLegDL[] = dmm_gRealBombchuBackRightLegDL;

#define dmm_gRealBombchuFrontRightLegDL "__OTR__mm/objects/object_rat/gRealBombchuFrontRightLegDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuFrontRightLegDL[] = dmm_gRealBombchuFrontRightLegDL;

#define dmm_gRealBombchuBackLeftLegDL "__OTR__mm/objects/object_rat/gRealBombchuBackLeftLegDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuBackLeftLegDL[] = dmm_gRealBombchuBackLeftLegDL;

#define dmm_gRealBombchuFrontLeftLegDL "__OTR__mm/objects/object_rat/gRealBombchuFrontLeftLegDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuFrontLeftLegDL[] = dmm_gRealBombchuFrontLeftLegDL;

#define dmm_gRealBombchuTailBaseDL "__OTR__mm/objects/object_rat/gRealBombchuTailBaseDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuTailBaseDL[] = dmm_gRealBombchuTailBaseDL;

#define dmm_gRealBombchuTailMiddleDL "__OTR__mm/objects/object_rat/gRealBombchuTailMiddleDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuTailMiddleDL[] = dmm_gRealBombchuTailMiddleDL;

#define dmm_gRealBombchuTailEndDL "__OTR__mm/objects/object_rat/gRealBombchuTailEndDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuTailEndDL[] = dmm_gRealBombchuTailEndDL;

#define dmm_gRealBombchuHeadDL "__OTR__mm/objects/object_rat/gRealBombchuHeadDL"
static const ALIGN_ASSET(2) char mm_gRealBombchuHeadDL[] = dmm_gRealBombchuHeadDL;

#define dmm_gRealBombchuSkinTex "__OTR__mm/objects/object_rat/gRealBombchuSkinTex"
static const ALIGN_ASSET(2) char mm_gRealBombchuSkinTex[] = dmm_gRealBombchuSkinTex;

#define dmm_gRealBombchuMouthTex "__OTR__mm/objects/object_rat/gRealBombchuMouthTex"
static const ALIGN_ASSET(2) char mm_gRealBombchuMouthTex[] = dmm_gRealBombchuMouthTex;

#define dmm_gRealBombchuEyeTex "__OTR__mm/objects/object_rat/gRealBombchuEyeTex"
static const ALIGN_ASSET(2) char mm_gRealBombchuEyeTex[] = dmm_gRealBombchuEyeTex;

#define dmm_gRealBombchuSkel "__OTR__mm/objects/object_rat/gRealBombchuSkel"
static const ALIGN_ASSET(2) char mm_gRealBombchuSkel[] = dmm_gRealBombchuSkel;

typedef enum RealBombchuLimb {
    /* 0x00 */ REAL_BOMBCHU_LIMB_NONE,
    /* 0x01 */ REAL_BOMBCHU_LIMB_BODY,
    /* 0x02 */ REAL_BOMBCHU_LIMB_HEAD,
    /* 0x03 */ REAL_BOMBCHU_LIMB_TAIL_BASE,
    /* 0x04 */ REAL_BOMBCHU_LIMB_TAIL_MIDDLE,
    /* 0x05 */ REAL_BOMBCHU_LIMB_TAIL_END,
    /* 0x06 */ REAL_BOMBCHU_LIMB_FRONT_LEFT_LEG,
    /* 0x07 */ REAL_BOMBCHU_LIMB_BACK_LEFT_LEG,
    /* 0x08 */ REAL_BOMBCHU_LIMB_FRONT_RIGHT_LEG,
    /* 0x09 */ REAL_BOMBCHU_LIMB_BACK_RIGHT_LEG,
    /* 0x0A */ REAL_BOMBCHU_LIMB_MAX
} RealBombchuLimb;

#endif // MM_OBJECTS_OBJECT_RAT_H
