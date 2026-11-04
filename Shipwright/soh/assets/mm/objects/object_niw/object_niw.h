#ifndef MM_OBJECTS_OBJECT_NIW_H
#define MM_OBJECTS_OBJECT_NIW_H 1

#include "align_asset_macro.h"

#define dmm_gNiwIdleAnim "__OTR__mm/objects/object_niw/gNiwIdleAnim"
static const ALIGN_ASSET(2) char mm_gNiwIdleAnim[] = dmm_gNiwIdleAnim;

#define dmm_gNiwNeckDL "__OTR__mm/objects/object_niw/gNiwNeckDL"
static const ALIGN_ASSET(2) char mm_gNiwNeckDL[] = dmm_gNiwNeckDL;

#define dmm_gNiwHeadDL "__OTR__mm/objects/object_niw/gNiwHeadDL"
static const ALIGN_ASSET(2) char mm_gNiwHeadDL[] = dmm_gNiwHeadDL;

#define dmm_gNiwTailDL "__OTR__mm/objects/object_niw/gNiwTailDL"
static const ALIGN_ASSET(2) char mm_gNiwTailDL[] = dmm_gNiwTailDL;

#define dmm_gNiwWingLeftDL "__OTR__mm/objects/object_niw/gNiwWingLeftDL"
static const ALIGN_ASSET(2) char mm_gNiwWingLeftDL[] = dmm_gNiwWingLeftDL;

#define dmm_gNiwFootLeftDL "__OTR__mm/objects/object_niw/gNiwFootLeftDL"
static const ALIGN_ASSET(2) char mm_gNiwFootLeftDL[] = dmm_gNiwFootLeftDL;

#define dmm_gNiwWingRightDL "__OTR__mm/objects/object_niw/gNiwWingRightDL"
static const ALIGN_ASSET(2) char mm_gNiwWingRightDL[] = dmm_gNiwWingRightDL;

#define dmm_gNiwFootRightDL "__OTR__mm/objects/object_niw/gNiwFootRightDL"
static const ALIGN_ASSET(2) char mm_gNiwFootRightDL[] = dmm_gNiwFootRightDL;

#define dmm_gNiwEyeTex "__OTR__mm/objects/object_niw/gNiwEyeTex"
static const ALIGN_ASSET(2) char mm_gNiwEyeTex[] = dmm_gNiwEyeTex;

#define dmm_gNiwHeadFeathersTex "__OTR__mm/objects/object_niw/gNiwHeadFeathersTex"
static const ALIGN_ASSET(2) char mm_gNiwHeadFeathersTex[] = dmm_gNiwHeadFeathersTex;

#define dmm_gNiwBaseFeathersTex "__OTR__mm/objects/object_niw/gNiwBaseFeathersTex"
static const ALIGN_ASSET(2) char mm_gNiwBaseFeathersTex[] = dmm_gNiwBaseFeathersTex;

#define dmm_gNiwBeakTex "__OTR__mm/objects/object_niw/gNiwBeakTex"
static const ALIGN_ASSET(2) char mm_gNiwBeakTex[] = dmm_gNiwBeakTex;

#define dmm_gNiwCombTex "__OTR__mm/objects/object_niw/gNiwCombTex"
static const ALIGN_ASSET(2) char mm_gNiwCombTex[] = dmm_gNiwCombTex;

#define dmm_gNiwTailFeathersTex "__OTR__mm/objects/object_niw/gNiwTailFeathersTex"
static const ALIGN_ASSET(2) char mm_gNiwTailFeathersTex[] = dmm_gNiwTailFeathersTex;

#define dmm_gNiwFootTex "__OTR__mm/objects/object_niw/gNiwFootTex"
static const ALIGN_ASSET(2) char mm_gNiwFootTex[] = dmm_gNiwFootTex;

#define dmm_gNiwFeatherTex "__OTR__mm/objects/object_niw/gNiwFeatherTex"
static const ALIGN_ASSET(2) char mm_gNiwFeatherTex[] = dmm_gNiwFeatherTex;

#define dmm_gNiwFeatherMaterialDL "__OTR__mm/objects/object_niw/gNiwFeatherMaterialDL"
static const ALIGN_ASSET(2) char mm_gNiwFeatherMaterialDL[] = dmm_gNiwFeatherMaterialDL;

#define dmm_gNiwFeatherDL "__OTR__mm/objects/object_niw/gNiwFeatherDL"
static const ALIGN_ASSET(2) char mm_gNiwFeatherDL[] = dmm_gNiwFeatherDL;

#define dmm_gNiwSkel "__OTR__mm/objects/object_niw/gNiwSkel"
static const ALIGN_ASSET(2) char mm_gNiwSkel[] = dmm_gNiwSkel;

typedef enum ObjectNiwLimb {
    /* 0x00 */ NIW_LIMB_NONE,
    /* 0x01 */ NIW_LIMB_ROOT,
    /* 0x02 */ NIW_LIMB_2,
    /* 0x03 */ NIW_LIMB_3,
    /* 0x04 */ NIW_LIMB_UNDER_SIDE,
    /* 0x05 */ NIW_LIMB_LEFT_FOOT_ROOT,
    /* 0x06 */ NIW_LIMB_LEFT_FOOT,
    /* 0x07 */ NIW_LIMB_LEFT_WING_ROOT,
    /* 0x08 */ NIW_LIMB_LEFT_WING,
    /* 0x09 */ NIW_LIMB_RIGHT_FOOT_ROOT,
    /* 0x0A */ NIW_LIMB_RIGHT_FOOT,
    /* 0x0B */ NIW_LIMB_RIGHT_WING_ROOT,
    /* 0x0C */ NIW_LIMB_RIGHT_WING,
    /* 0x0D */ NIW_LIMB_UPPER_BODY,
    /* 0x0E */ NIW_LIMB_NECK,
    /* 0x0F */ NIW_LIMB_HEAD,
    /* 0x10 */ NIW_LIMB_MAX
} ObjectNiwLimb;

#endif // MM_OBJECTS_OBJECT_NIW_H
