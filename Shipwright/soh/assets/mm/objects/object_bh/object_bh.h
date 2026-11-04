#ifndef MM_OBJECTS_OBJECT_BH_H
#define MM_OBJECTS_OBJECT_BH_H 1

#include "align_asset_macro.h"

#define dmm_gBhFlyingAnim "__OTR__mm/objects/object_bh/gBhFlyingAnim"
static const ALIGN_ASSET(2) char mm_gBhFlyingAnim[] = dmm_gBhFlyingAnim;

#define dmm_gBhBodyDL "__OTR__mm/objects/object_bh/gBhBodyDL"
static const ALIGN_ASSET(2) char mm_gBhBodyDL[] = dmm_gBhBodyDL;

#define dmm_gBhRightWingToEdgeDL "__OTR__mm/objects/object_bh/gBhRightWingToEdgeDL"
static const ALIGN_ASSET(2) char mm_gBhRightWingToEdgeDL[] = dmm_gBhRightWingToEdgeDL;

#define dmm_gBhRightWingToBodyDL "__OTR__mm/objects/object_bh/gBhRightWingToBodyDL"
static const ALIGN_ASSET(2) char mm_gBhRightWingToBodyDL[] = dmm_gBhRightWingToBodyDL;

#define dmm_gBhLeftWingToEdgeDL "__OTR__mm/objects/object_bh/gBhLeftWingToEdgeDL"
static const ALIGN_ASSET(2) char mm_gBhLeftWingToEdgeDL[] = dmm_gBhLeftWingToEdgeDL;

#define dmm_gBhLeftWingToBodyDL "__OTR__mm/objects/object_bh/gBhLeftWingToBodyDL"
static const ALIGN_ASSET(2) char mm_gBhLeftWingToBodyDL[] = dmm_gBhLeftWingToBodyDL;

#define dmm_gBhBodyTex "__OTR__mm/objects/object_bh/gBhBodyTex"
static const ALIGN_ASSET(2) char mm_gBhBodyTex[] = dmm_gBhBodyTex;

#define dmm_gBhWingTex "__OTR__mm/objects/object_bh/gBhWingTex"
static const ALIGN_ASSET(2) char mm_gBhWingTex[] = dmm_gBhWingTex;

#define dmm_gBhSkel "__OTR__mm/objects/object_bh/gBhSkel"
static const ALIGN_ASSET(2) char mm_gBhSkel[] = dmm_gBhSkel;

typedef enum ObjectBhLimb {
    /* 0x00 */ OBJECT_BH_LIMB_NONE,
    /* 0x01 */ OBJECT_BH_LIMB_BODY,
    /* 0x02 */ OBJECT_BH_LIMB_RIGHT_WING_TO_BODY,
    /* 0x03 */ OBJECT_BH_LIMB_RIGHT_WING_TO_EDGE,
    /* 0x04 */ OBJECT_BH_LIMB_LEFT_WING_TO_BODY,
    /* 0x05 */ OBJECT_BH_LIMB_LEFT_WING_TO_EDGE,
    /* 0x06 */ OBJECT_BH_LIMB_MAX
} ObjectBhLimb;

#endif // MM_OBJECTS_OBJECT_BH_H
