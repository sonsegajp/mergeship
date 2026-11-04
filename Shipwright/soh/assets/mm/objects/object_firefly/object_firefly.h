#ifndef MM_OBJECTS_OBJECT_FIREFLY_H
#define MM_OBJECTS_OBJECT_FIREFLY_H 1

#include "align_asset_macro.h"

#define dmm_gFireKeeseFlyAnim "__OTR__mm/objects/object_firefly/gFireKeeseFlyAnim"
static const ALIGN_ASSET(2) char mm_gFireKeeseFlyAnim[] = dmm_gFireKeeseFlyAnim;

#define dmm_gFireKeeseBodyTex "__OTR__mm/objects/object_firefly/gFireKeeseBodyTex"
static const ALIGN_ASSET(2) char mm_gFireKeeseBodyTex[] = dmm_gFireKeeseBodyTex;

#define dmm_gFireKeeseEyeTex "__OTR__mm/objects/object_firefly/gFireKeeseEyeTex"
static const ALIGN_ASSET(2) char mm_gFireKeeseEyeTex[] = dmm_gFireKeeseEyeTex;

#define dmm_gFireKeeseEarTex "__OTR__mm/objects/object_firefly/gFireKeeseEarTex"
static const ALIGN_ASSET(2) char mm_gFireKeeseEarTex[] = dmm_gFireKeeseEarTex;

#define dmm_gFireKeeseTalonTex "__OTR__mm/objects/object_firefly/gFireKeeseTalonTex"
static const ALIGN_ASSET(2) char mm_gFireKeeseTalonTex[] = dmm_gFireKeeseTalonTex;

#define dmm_gFireKeeseWingTex "__OTR__mm/objects/object_firefly/gFireKeeseWingTex"
static const ALIGN_ASSET(2) char mm_gFireKeeseWingTex[] = dmm_gFireKeeseWingTex;

#define dmm_gKeeseRedEyeTex "__OTR__mm/objects/object_firefly/gKeeseRedEyeTex"
static const ALIGN_ASSET(2) char mm_gKeeseRedEyeTex[] = dmm_gKeeseRedEyeTex;

#define dmm_gKeeseBodyTex "__OTR__mm/objects/object_firefly/gKeeseBodyTex"
static const ALIGN_ASSET(2) char mm_gKeeseBodyTex[] = dmm_gKeeseBodyTex;

#define dmm_gKeeseTalonTex "__OTR__mm/objects/object_firefly/gKeeseTalonTex"
static const ALIGN_ASSET(2) char mm_gKeeseTalonTex[] = dmm_gKeeseTalonTex;

#define dmm_gKeeseWingTex "__OTR__mm/objects/object_firefly/gKeeseWingTex"
static const ALIGN_ASSET(2) char mm_gKeeseWingTex[] = dmm_gKeeseWingTex;

#define dmm_gKeeseEarTex "__OTR__mm/objects/object_firefly/gKeeseEarTex"
static const ALIGN_ASSET(2) char mm_gKeeseEarTex[] = dmm_gKeeseEarTex;

#define dmm_gFireKeeseHeadDL "__OTR__mm/objects/object_firefly/gFireKeeseHeadDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseHeadDL[] = dmm_gFireKeeseHeadDL;

#define dmm_gFireKeeseBodyDL "__OTR__mm/objects/object_firefly/gFireKeeseBodyDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseBodyDL[] = dmm_gFireKeeseBodyDL;

#define dmm_gFireKeeseLeftFootDL "__OTR__mm/objects/object_firefly/gFireKeeseLeftFootDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseLeftFootDL[] = dmm_gFireKeeseLeftFootDL;

#define dmm_gFireKeeseRightFootDL "__OTR__mm/objects/object_firefly/gFireKeeseRightFootDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseRightFootDL[] = dmm_gFireKeeseRightFootDL;

#define dmm_gFireKeeseRightWingStartDL "__OTR__mm/objects/object_firefly/gFireKeeseRightWingStartDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseRightWingStartDL[] = dmm_gFireKeeseRightWingStartDL;

#define dmm_gFireKeeseRightWingMidDL "__OTR__mm/objects/object_firefly/gFireKeeseRightWingMidDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseRightWingMidDL[] = dmm_gFireKeeseRightWingMidDL;

#define dmm_gFireKeeseRightWingEndDL "__OTR__mm/objects/object_firefly/gFireKeeseRightWingEndDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseRightWingEndDL[] = dmm_gFireKeeseRightWingEndDL;

#define dmm_gFireKeeseLeftWingEndDL "__OTR__mm/objects/object_firefly/gFireKeeseLeftWingEndDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseLeftWingEndDL[] = dmm_gFireKeeseLeftWingEndDL;

#define dmm_gFireKeeseLeftWingMidDL "__OTR__mm/objects/object_firefly/gFireKeeseLeftWingMidDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseLeftWingMidDL[] = dmm_gFireKeeseLeftWingMidDL;

#define dmm_gFireKeeseLeftWingStartDL "__OTR__mm/objects/object_firefly/gFireKeeseLeftWingStartDL"
static const ALIGN_ASSET(2) char mm_gFireKeeseLeftWingStartDL[] = dmm_gFireKeeseLeftWingStartDL;

#define dmm_gKeeseRedEyesDL "__OTR__mm/objects/object_firefly/gKeeseRedEyesDL"
static const ALIGN_ASSET(2) char mm_gKeeseRedEyesDL[] = dmm_gKeeseRedEyesDL;

#define dmm_gFireKeeseSkel "__OTR__mm/objects/object_firefly/gFireKeeseSkel"
static const ALIGN_ASSET(2) char mm_gFireKeeseSkel[] = dmm_gFireKeeseSkel;

typedef enum FireKeeseLimb {
    /* 0x00 */ FIRE_KEESE_LIMB_NONE,
    /* 0x01 */ FIRE_KEESE_LIMB_ROOT,
    /* 0x02 */ FIRE_KEESE_LIMB_ROOT_WRAPPER,
    /* 0x03 */ FIRE_KEESE_LIMB_FEET_ROOT,
    /* 0x04 */ FIRE_KEESE_LIMB_RIGHT_FOOT_ROOT,
    /* 0x05 */ FIRE_KEESE_LIMB_RIGHT_FOOT_WRAPPER,
    /* 0x06 */ FIRE_KEESE_LIMB_RIGHT_FOOT,
    /* 0x07 */ FIRE_KEESE_LIMB_LEFT_FOOT_ROOT,
    /* 0x08 */ FIRE_KEESE_LIMB_LEFT_FOOT_WRAPPER,
    /* 0x09 */ FIRE_KEESE_LIMB_LEFT_FOOT,
    /* 0x0A */ FIRE_KEESE_LIMB_BODY,
    /* 0x0B */ FIRE_KEESE_LIMB_LEFT_WING_ROOT,
    /* 0x0C */ FIRE_KEESE_LIMB_LEFT_WING_WRAPPER,
    /* 0x0D */ FIRE_KEESE_LIMB_LEFT_WING_END_MID_ROOT,
    /* 0x0E */ FIRE_KEESE_LIMB_LEFT_WING_END_ROOT,
    /* 0x0F */ FIRE_KEESE_LIMB_LEFT_WING_END,
    /* 0x10 */ FIRE_KEESE_LIMB_LEFT_WING_MID,
    /* 0x11 */ FIRE_KEESE_LIMB_LEFT_WING_START,
    /* 0x12 */ FIRE_KEESE_LIMB_RIGHT_WING_ROOT,
    /* 0x13 */ FIRE_KEESE_LIMB_RIGHT_WING_WRAPPER,
    /* 0x14 */ FIRE_KEESE_LIMB_RIGHT_WING_END_MID_ROOT,
    /* 0x15 */ FIRE_KEESE_LIMB_RIGHT_WING_END_ROOT,
    /* 0x16 */ FIRE_KEESE_LIMB_RIGHT_WING_END,
    /* 0x17 */ FIRE_KEESE_LIMB_RIGHT_WING_MID,
    /* 0x18 */ FIRE_KEESE_LIMB_RIGHT_WING_START,
    /* 0x19 */ FIRE_KEESE_LIMB_HEAD_ROOT,
    /* 0x1A */ FIRE_KEESE_LIMB_HEAD_WRAPPER,
    /* 0x1B */ FIRE_KEESE_LIMB_HEAD,
    /* 0x1C */ FIRE_KEESE_LIMB_MAX
} FireKeeseLimb;

#endif // MM_OBJECTS_OBJECT_FIREFLY_H
