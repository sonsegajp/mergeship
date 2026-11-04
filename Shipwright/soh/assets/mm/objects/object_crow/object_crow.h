#ifndef MM_OBJECTS_OBJECT_CROW_H
#define MM_OBJECTS_OBJECT_CROW_H 1

#include "align_asset_macro.h"

#define dmm_gGuayFlyAnim "__OTR__mm/objects/object_crow/gGuayFlyAnim"
static const ALIGN_ASSET(2) char mm_gGuayFlyAnim[] = dmm_gGuayFlyAnim;

#define dmm_gGuayBodyDL "__OTR__mm/objects/object_crow/gGuayBodyDL"
static const ALIGN_ASSET(2) char mm_gGuayBodyDL[] = dmm_gGuayBodyDL;

#define dmm_gGuayRightWingTipDL "__OTR__mm/objects/object_crow/gGuayRightWingTipDL"
static const ALIGN_ASSET(2) char mm_gGuayRightWingTipDL[] = dmm_gGuayRightWingTipDL;

#define dmm_gGuayRightWingBodyDL "__OTR__mm/objects/object_crow/gGuayRightWingBodyDL"
static const ALIGN_ASSET(2) char mm_gGuayRightWingBodyDL[] = dmm_gGuayRightWingBodyDL;

#define dmm_gGuayLeftWingTipDL "__OTR__mm/objects/object_crow/gGuayLeftWingTipDL"
static const ALIGN_ASSET(2) char mm_gGuayLeftWingTipDL[] = dmm_gGuayLeftWingTipDL;

#define dmm_gGuayLeftWingBodyDL "__OTR__mm/objects/object_crow/gGuayLeftWingBodyDL"
static const ALIGN_ASSET(2) char mm_gGuayLeftWingBodyDL[] = dmm_gGuayLeftWingBodyDL;

#define dmm_gGuayTailDL "__OTR__mm/objects/object_crow/gGuayTailDL"
static const ALIGN_ASSET(2) char mm_gGuayTailDL[] = dmm_gGuayTailDL;

#define dmm_gGuayUpperTailDL "__OTR__mm/objects/object_crow/gGuayUpperTailDL"
static const ALIGN_ASSET(2) char mm_gGuayUpperTailDL[] = dmm_gGuayUpperTailDL;

#define dmm_gGuayBodyTex "__OTR__mm/objects/object_crow/gGuayBodyTex"
static const ALIGN_ASSET(2) char mm_gGuayBodyTex[] = dmm_gGuayBodyTex;

#define dmm_gGuayEyeTex "__OTR__mm/objects/object_crow/gGuayEyeTex"
static const ALIGN_ASSET(2) char mm_gGuayEyeTex[] = dmm_gGuayEyeTex;

#define dmm_gGuayTailTex "__OTR__mm/objects/object_crow/gGuayTailTex"
static const ALIGN_ASSET(2) char mm_gGuayTailTex[] = dmm_gGuayTailTex;

#define dmm_gGuaySkel "__OTR__mm/objects/object_crow/gGuaySkel"
static const ALIGN_ASSET(2) char mm_gGuaySkel[] = dmm_gGuaySkel;

typedef enum ObjectCrowLimb {
    /* 0x00 */ OBJECT_CROW_LIMB_NONE,
    /* 0x01 */ OBJECT_CROW_LIMB_ROOT,
    /* 0x02 */ OBJECT_CROW_LIMB_BODY,
    /* 0x03 */ OBJECT_CROW_LIMB_RIGHT_WING_BODY,
    /* 0x04 */ OBJECT_CROW_LIMB_RIGHT_WING_TIP,
    /* 0x05 */ OBJECT_CROW_LIMB_LEFT_WING_BODY,
    /* 0x06 */ OBJECT_CROW_LIMB_LEFT_WING_TIP,
    /* 0x07 */ OBJECT_CROW_LIMB_UPPER_TAIL,
    /* 0x08 */ OBJECT_CROW_LIMB_TAIL,
    /* 0x09 */ OBJECT_CROW_LIMB_MAX
} ObjectCrowLimb;

#endif // MM_OBJECTS_OBJECT_CROW_H
