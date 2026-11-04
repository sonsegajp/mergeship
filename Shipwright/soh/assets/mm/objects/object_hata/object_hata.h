#ifndef MM_OBJECTS_OBJECT_HATA_H
#define MM_OBJECTS_OBJECT_HATA_H 1

#include "align_asset_macro.h"

#define dmm_gFlagpoleCol "__OTR__mm/objects/object_hata/gFlagpoleCol"
static const ALIGN_ASSET(2) char mm_gFlagpoleCol[] = dmm_gFlagpoleCol;

#define dmm_gFlagpoleFlapAnim "__OTR__mm/objects/object_hata/gFlagpoleFlapAnim"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlapAnim[] = dmm_gFlagpoleFlapAnim;

#define dmm_gFlagpolePoleTex "__OTR__mm/objects/object_hata/gFlagpolePoleTex"
static const ALIGN_ASSET(2) char mm_gFlagpolePoleTex[] = dmm_gFlagpolePoleTex;

#define dmm_gFlagpoleTopTex "__OTR__mm/objects/object_hata/gFlagpoleTopTex"
static const ALIGN_ASSET(2) char mm_gFlagpoleTopTex[] = dmm_gFlagpoleTopTex;

#define dmm_gFlagpoleFlagTex "__OTR__mm/objects/object_hata/gFlagpoleFlagTex"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlagTex[] = dmm_gFlagpoleFlagTex;

#define dmm_gFlagpolePoleDL "__OTR__mm/objects/object_hata/gFlagpolePoleDL"
static const ALIGN_ASSET(2) char mm_gFlagpolePoleDL[] = dmm_gFlagpolePoleDL;

#define dmm_gFlagpoleFlag1FlyEndDL "__OTR__mm/objects/object_hata/gFlagpoleFlag1FlyEndDL"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlag1FlyEndDL[] = dmm_gFlagpoleFlag1FlyEndDL;

#define dmm_gFlagpoleFlag1FlyMidDL "__OTR__mm/objects/object_hata/gFlagpoleFlag1FlyMidDL"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlag1FlyMidDL[] = dmm_gFlagpoleFlag1FlyMidDL;

#define dmm_gFlagpoleFlag1HoistMidDL "__OTR__mm/objects/object_hata/gFlagpoleFlag1HoistMidDL"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlag1HoistMidDL[] = dmm_gFlagpoleFlag1HoistMidDL;

#define dmm_gFlagpoleFlag1HoistEndDL "__OTR__mm/objects/object_hata/gFlagpoleFlag1HoistEndDL"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlag1HoistEndDL[] = dmm_gFlagpoleFlag1HoistEndDL;

#define dmm_gFlagpoleFlag2FlyEndDL "__OTR__mm/objects/object_hata/gFlagpoleFlag2FlyEndDL"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlag2FlyEndDL[] = dmm_gFlagpoleFlag2FlyEndDL;

#define dmm_gFlagpoleFlag2FlyMidDL "__OTR__mm/objects/object_hata/gFlagpoleFlag2FlyMidDL"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlag2FlyMidDL[] = dmm_gFlagpoleFlag2FlyMidDL;

#define dmm_gFlagpoleFlag2HoistMidDL "__OTR__mm/objects/object_hata/gFlagpoleFlag2HoistMidDL"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlag2HoistMidDL[] = dmm_gFlagpoleFlag2HoistMidDL;

#define dmm_gFlagpoleFlag2HoistEndDL "__OTR__mm/objects/object_hata/gFlagpoleFlag2HoistEndDL"
static const ALIGN_ASSET(2) char mm_gFlagpoleFlag2HoistEndDL[] = dmm_gFlagpoleFlag2HoistEndDL;

#define dmm_gFlagpoleSkel "__OTR__mm/objects/object_hata/gFlagpoleSkel"
static const ALIGN_ASSET(2) char mm_gFlagpoleSkel[] = dmm_gFlagpoleSkel;

typedef enum FlagpoleLimb {
    /* 0x00 */ FLAGPOLE_LIMB_NONE,
    /* 0x01 */ FLAGPOLE_LIMB_BASE,
    /* 0x02 */ FLAGPOLE_LIMB_POLE,
    /* 0x03 */ FLAGPOLE_LIMB_FLAG1_BASE,
    /* 0x04 */ FLAGPOLE_LIMB_FLAG1_HOIST_END_BASE,
    /* 0x05 */ FLAGPOLE_LIMB_FLAG1_HOIST_MID_BASE,
    /* 0x06 */ FLAGPOLE_LIMB_FLAG1_FLY_MID_BASE,
    /* 0x07 */ FLAGPOLE_LIMB_FLAG1_FLY_END_BASE,
    /* 0x08 */ FLAGPOLE_LIMB_FLAG1_FLY_END,
    /* 0x09 */ FLAGPOLE_LIMB_FLAG1_FLY_MID,
    /* 0x0A */ FLAGPOLE_LIMB_FLAG1_HOIST_MID,
    /* 0x0B */ FLAGPOLE_LIMB_FLAG1_HOIST_END,
    /* 0x0C */ FLAGPOLE_LIMB_FLAG2_BASE,
    /* 0x0D */ FLAGPOLE_LIMB_FLAG2_HOIST_END_BASE,
    /* 0x0E */ FLAGPOLE_LIMB_FLAG2_HOIST_MID_BASE,
    /* 0x0F */ FLAGPOLE_LIMB_FLAG2_FLY_MID_BASE,
    /* 0x10 */ FLAGPOLE_LIMB_FLAG2_FLY_END_BASE,
    /* 0x11 */ FLAGPOLE_LIMB_FLAG2_FLY_END,
    /* 0x12 */ FLAGPOLE_LIMB_FLAG2_FLY_MID,
    /* 0x13 */ FLAGPOLE_LIMB_FLAG2_HOIST_MID,
    /* 0x14 */ FLAGPOLE_LIMB_FLAG2_HOIST_END,
    /* 0x15 */ FLAGPOLE_LIMB_MAX
} FlagpoleLimb;

#endif // MM_OBJECTS_OBJECT_HATA_H
