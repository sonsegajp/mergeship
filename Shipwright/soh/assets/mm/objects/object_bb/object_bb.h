#ifndef MM_OBJECTS_OBJECT_BB_H
#define MM_OBJECTS_OBJECT_BB_H 1

#include "align_asset_macro.h"

#define dmm_gBubbleAttackAnim "__OTR__mm/objects/object_bb/gBubbleAttackAnim"
static const ALIGN_ASSET(2) char mm_gBubbleAttackAnim[] = dmm_gBubbleAttackAnim;

#define dmm_gBubblePainAnim "__OTR__mm/objects/object_bb/gBubblePainAnim"
static const ALIGN_ASSET(2) char mm_gBubblePainAnim[] = dmm_gBubblePainAnim;

#define dmm_gBubbleFlyingAnim "__OTR__mm/objects/object_bb/gBubbleFlyingAnim"
static const ALIGN_ASSET(2) char mm_gBubbleFlyingAnim[] = dmm_gBubbleFlyingAnim;

#define dmm_gBubbleLeftWingWebbingDL "__OTR__mm/objects/object_bb/gBubbleLeftWingWebbingDL"
static const ALIGN_ASSET(2) char mm_gBubbleLeftWingWebbingDL[] = dmm_gBubbleLeftWingWebbingDL;

#define dmm_gBubbleLeftWingBoneDL "__OTR__mm/objects/object_bb/gBubbleLeftWingBoneDL"
static const ALIGN_ASSET(2) char mm_gBubbleLeftWingBoneDL[] = dmm_gBubbleLeftWingBoneDL;

#define dmm_gBubbleJawDL "__OTR__mm/objects/object_bb/gBubbleJawDL"
static const ALIGN_ASSET(2) char mm_gBubbleJawDL[] = dmm_gBubbleJawDL;

#define dmm_gBubbleCraniumDL "__OTR__mm/objects/object_bb/gBubbleCraniumDL"
static const ALIGN_ASSET(2) char mm_gBubbleCraniumDL[] = dmm_gBubbleCraniumDL;

#define dmm_gBubbleRightWingBoneDL "__OTR__mm/objects/object_bb/gBubbleRightWingBoneDL"
static const ALIGN_ASSET(2) char mm_gBubbleRightWingBoneDL[] = dmm_gBubbleRightWingBoneDL;

#define dmm_gBubbleRightWingWebbingDL "__OTR__mm/objects/object_bb/gBubbleRightWingWebbingDL"
static const ALIGN_ASSET(2) char mm_gBubbleRightWingWebbingDL[] = dmm_gBubbleRightWingWebbingDL;

#define dmm_gBubbleEyeSocketBottomTex "__OTR__mm/objects/object_bb/gBubbleEyeSocketBottomTex"
static const ALIGN_ASSET(2) char mm_gBubbleEyeSocketBottomTex[] = dmm_gBubbleEyeSocketBottomTex;

#define dmm_gBubbleNostrilTex "__OTR__mm/objects/object_bb/gBubbleNostrilTex"
static const ALIGN_ASSET(2) char mm_gBubbleNostrilTex[] = dmm_gBubbleNostrilTex;

#define dmm_gBubbleEyeSocketTopAndJawTex "__OTR__mm/objects/object_bb/gBubbleEyeSocketTopAndJawTex"
static const ALIGN_ASSET(2) char mm_gBubbleEyeSocketTopAndJawTex[] = dmm_gBubbleEyeSocketTopAndJawTex;

#define dmm_gBubbleCraniumTopTex "__OTR__mm/objects/object_bb/gBubbleCraniumTopTex"
static const ALIGN_ASSET(2) char mm_gBubbleCraniumTopTex[] = dmm_gBubbleCraniumTopTex;

#define dmm_gBubbleCraniumSidesAndBackTex "__OTR__mm/objects/object_bb/gBubbleCraniumSidesAndBackTex"
static const ALIGN_ASSET(2) char mm_gBubbleCraniumSidesAndBackTex[] = dmm_gBubbleCraniumSidesAndBackTex;

#define dmm_gBubbleEyeGlowTex "__OTR__mm/objects/object_bb/gBubbleEyeGlowTex"
static const ALIGN_ASSET(2) char mm_gBubbleEyeGlowTex[] = dmm_gBubbleEyeGlowTex;

#define dmm_gBubbleWingBoneTex "__OTR__mm/objects/object_bb/gBubbleWingBoneTex"
static const ALIGN_ASSET(2) char mm_gBubbleWingBoneTex[] = dmm_gBubbleWingBoneTex;

#define dmm_gBubbleWingWebbingTex "__OTR__mm/objects/object_bb/gBubbleWingWebbingTex"
static const ALIGN_ASSET(2) char mm_gBubbleWingWebbingTex[] = dmm_gBubbleWingWebbingTex;

#define dmm_gBubbleSkel "__OTR__mm/objects/object_bb/gBubbleSkel"
static const ALIGN_ASSET(2) char mm_gBubbleSkel[] = dmm_gBubbleSkel;

typedef enum BubbleLimb {
    /* 0x00 */ BUBBLE_LIMB_NONE,
    /* 0x01 */ BUBBLE_LIMB_ROOT,
    /* 0x02 */ BUBBLE_LIMB_CRANIUM_ROOT,
    /* 0x03 */ BUBBLE_LIMB_JAW_ROOT,
    /* 0x04 */ BUBBLE_LIMB_JAW,
    /* 0x05 */ BUBBLE_LIMB_LEFT_WING_ROOT,
    /* 0x06 */ BUBBLE_LIMB_LEFT_WING_WRAPPER,
    /* 0x07 */ BUBBLE_LIMB_LEFT_WING_WEBBING_ROOT,
    /* 0x08 */ BUBBLE_LIMB_LEFT_WING_WEBBING,
    /* 0x09 */ BUBBLE_LIMB_LEFT_WING_BONE,
    /* 0x0A */ BUBBLE_LIMB_RIGHT_WING_ROOT,
    /* 0x0B */ BUBBLE_LIMB_RIGHT_WING_WRAPPER,
    /* 0x0C */ BUBBLE_LIMB_RIGHT_WING_WEBBING_ROOT,
    /* 0x0D */ BUBBLE_LIMB_RIGHT_WING_WEBBING,
    /* 0x0E */ BUBBLE_LIMB_RIGHT_WING_BONE,
    /* 0x0F */ BUBBLE_LIMB_CRANIUM,
    /* 0x10 */ BUBBLE_LIMB_MAX
} BubbleLimb;

#endif // MM_OBJECTS_OBJECT_BB_H
