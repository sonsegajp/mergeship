#ifndef MM_OBJECTS_OBJECT_BJT_H
#define MM_OBJECTS_OBJECT_BJT_H 1

#include "align_asset_macro.h"

#define dmm_gToiletHandWaggingFingerAnim "__OTR__mm/objects/object_bjt/gToiletHandWaggingFingerAnim"
static const ALIGN_ASSET(2) char mm_gToiletHandWaggingFingerAnim[] = dmm_gToiletHandWaggingFingerAnim;

#define dmm_gToiletHandFistAnim "__OTR__mm/objects/object_bjt/gToiletHandFistAnim"
static const ALIGN_ASSET(2) char mm_gToiletHandFistAnim[] = dmm_gToiletHandFistAnim;

#define dmm_gToiletHandThumbsUpAnim "__OTR__mm/objects/object_bjt/gToiletHandThumbsUpAnim"
static const ALIGN_ASSET(2) char mm_gToiletHandThumbsUpAnim[] = dmm_gToiletHandThumbsUpAnim;

#define dmm_gToiletHandOpenHandAnim "__OTR__mm/objects/object_bjt/gToiletHandOpenHandAnim"
static const ALIGN_ASSET(2) char mm_gToiletHandOpenHandAnim[] = dmm_gToiletHandOpenHandAnim;

#define dmm_gToiletHandWaitingAnim "__OTR__mm/objects/object_bjt/gToiletHandWaitingAnim"
static const ALIGN_ASSET(2) char mm_gToiletHandWaitingAnim[] = dmm_gToiletHandWaitingAnim;

#define dmm_gToiletHandIndexFingerDL "__OTR__mm/objects/object_bjt/gToiletHandIndexFingerDL"
static const ALIGN_ASSET(2) char mm_gToiletHandIndexFingerDL[] = dmm_gToiletHandIndexFingerDL;

#define dmm_gToiletHandThumbDL "__OTR__mm/objects/object_bjt/gToiletHandThumbDL"
static const ALIGN_ASSET(2) char mm_gToiletHandThumbDL[] = dmm_gToiletHandThumbDL;

#define dmm_gToiletHandOtherFingersDL "__OTR__mm/objects/object_bjt/gToiletHandOtherFingersDL"
static const ALIGN_ASSET(2) char mm_gToiletHandOtherFingersDL[] = dmm_gToiletHandOtherFingersDL;

#define dmm_gToiletHandPalmDL "__OTR__mm/objects/object_bjt/gToiletHandPalmDL"
static const ALIGN_ASSET(2) char mm_gToiletHandPalmDL[] = dmm_gToiletHandPalmDL;

#define dmm_gToiletHandArmDL "__OTR__mm/objects/object_bjt/gToiletHandArmDL"
static const ALIGN_ASSET(2) char mm_gToiletHandArmDL[] = dmm_gToiletHandArmDL;

#define dmm_gToiletHandFingersTex "__OTR__mm/objects/object_bjt/gToiletHandFingersTex"
static const ALIGN_ASSET(2) char mm_gToiletHandFingersTex[] = dmm_gToiletHandFingersTex;

#define dmm_gToiletHandArmSkinTex "__OTR__mm/objects/object_bjt/gToiletHandArmSkinTex"
static const ALIGN_ASSET(2) char mm_gToiletHandArmSkinTex[] = dmm_gToiletHandArmSkinTex;

#define dmm_gToiletHandSkel "__OTR__mm/objects/object_bjt/gToiletHandSkel"
static const ALIGN_ASSET(2) char mm_gToiletHandSkel[] = dmm_gToiletHandSkel;

typedef enum ToiletHandLimb {
    /* 0x00 */ TOILET_HAND_LIMB_NONE,
    /* 0x01 */ TOILET_HAND_LIMB_ROOT,
    /* 0x02 */ TOILET_HAND_LIMB_ARM,
    /* 0x03 */ TOILET_HAND_LIMB_PALM,
    /* 0x04 */ TOILET_HAND_LIMB_OTHER_FINGERS,
    /* 0x05 */ TOILET_HAND_LIMB_THUMB,
    /* 0x06 */ TOILET_HAND_LIMB_INDEX_FINGER,
    /* 0x07 */ TOILET_HAND_LIMB_MAX
} ToiletHandLimb;

#endif // MM_OBJECTS_OBJECT_BJT_H
