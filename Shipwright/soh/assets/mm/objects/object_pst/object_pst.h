#ifndef MM_OBJECTS_OBJECT_PST_H
#define MM_OBJECTS_OBJECT_PST_H 1

#include "align_asset_macro.h"

#define dmm_gPostboxIdleAnim "__OTR__mm/objects/object_pst/gPostboxIdleAnim"
static const ALIGN_ASSET(2) char mm_gPostboxIdleAnim[] = dmm_gPostboxIdleAnim;

#define dmm_gPostboxFrameDL "__OTR__mm/objects/object_pst/gPostboxFrameDL"
static const ALIGN_ASSET(2) char mm_gPostboxFrameDL[] = dmm_gPostboxFrameDL;

#define dmm_gPostboxMailSlotDL "__OTR__mm/objects/object_pst/gPostboxMailSlotDL"
static const ALIGN_ASSET(2) char mm_gPostboxMailSlotDL[] = dmm_gPostboxMailSlotDL;

#define dmm_gPostboxWoodRoofTex "__OTR__mm/objects/object_pst/gPostboxWoodRoofTex"
static const ALIGN_ASSET(2) char mm_gPostboxWoodRoofTex[] = dmm_gPostboxWoodRoofTex;

#define dmm_gPostboxWoodBodyTex "__OTR__mm/objects/object_pst/gPostboxWoodBodyTex"
static const ALIGN_ASSET(2) char mm_gPostboxWoodBodyTex[] = dmm_gPostboxWoodBodyTex;

#define dmm_gPostboxMetalTex "__OTR__mm/objects/object_pst/gPostboxMetalTex"
static const ALIGN_ASSET(2) char mm_gPostboxMetalTex[] = dmm_gPostboxMetalTex;

#define dmm_gPostboxSlotHoleTex "__OTR__mm/objects/object_pst/gPostboxSlotHoleTex"
static const ALIGN_ASSET(2) char mm_gPostboxSlotHoleTex[] = dmm_gPostboxSlotHoleTex;

#define dmm_gPostboxSkel "__OTR__mm/objects/object_pst/gPostboxSkel"
static const ALIGN_ASSET(2) char mm_gPostboxSkel[] = dmm_gPostboxSkel;

typedef enum PostboxLimb {
    /* 0x00 */ POSTBOX_LIMB_NONE,
    /* 0x01 */ POSTBOX_LIMB_FRAME,
    /* 0x02 */ POSTBOX_LIMB_MAIL_SLOT,
    /* 0x03 */ POSTBOX_LIMB_MAX
} PostboxLimb;

#endif // MM_OBJECTS_OBJECT_PST_H
