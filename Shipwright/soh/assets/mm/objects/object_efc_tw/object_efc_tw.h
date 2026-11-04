#ifndef MM_OBJECTS_OBJECT_EFC_TW_H
#define MM_OBJECTS_OBJECT_EFC_TW_H 1

#include "align_asset_macro.h"

#define dmm_gTimewarpAnim "__OTR__mm/objects/object_efc_tw/gTimewarpAnim"
static const ALIGN_ASSET(2) char mm_gTimewarpAnim[] = dmm_gTimewarpAnim;

#define dmm_gTimewarpVtx "__OTR__mm/objects/object_efc_tw/gTimewarpVtx"
static const ALIGN_ASSET(2) char mm_gTimewarpVtx[] = dmm_gTimewarpVtx;

#define dmm_gTimewarpDL "__OTR__mm/objects/object_efc_tw/gTimewarpDL"
static const ALIGN_ASSET(2) char mm_gTimewarpDL[] = dmm_gTimewarpDL;

#define dmm_gTimewarpTex "__OTR__mm/objects/object_efc_tw/gTimewarpTex"
static const ALIGN_ASSET(2) char mm_gTimewarpTex[] = dmm_gTimewarpTex;

#define dmm_gTimewarpSkel "__OTR__mm/objects/object_efc_tw/gTimewarpSkel"
static const ALIGN_ASSET(2) char mm_gTimewarpSkel[] = dmm_gTimewarpSkel;

typedef enum TimewarpLimb {
    /* 0x00 */ TIMEWARP_LIMB_NONE,
    /* 0x01 */ TIMEWARP_LIMB_1,
    /* 0x02 */ TIMEWARP_LIMB_2,
    /* 0x03 */ TIMEWARP_LIMB_MAX
} TimewarpLimb;

#endif // MM_OBJECTS_OBJECT_EFC_TW_H
