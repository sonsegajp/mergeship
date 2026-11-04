#ifndef MM_OBJECTS_OBJECT_STH_H
#define MM_OBJECTS_OBJECT_STH_H 1

#include "align_asset_macro.h"

#define dmm_gSthUnusedHeadVtx "__OTR__mm/objects/object_sth/gSthUnusedHeadVtx"
static const ALIGN_ASSET(2) char mm_gSthUnusedHeadVtx[] = dmm_gSthUnusedHeadVtx;

#define dmm_gSthHeadDL "__OTR__mm/objects/object_sth/gSthHeadDL"
static const ALIGN_ASSET(2) char mm_gSthHeadDL[] = dmm_gSthHeadDL;

#define dmm_gSthRightHandDL "__OTR__mm/objects/object_sth/gSthRightHandDL"
static const ALIGN_ASSET(2) char mm_gSthRightHandDL[] = dmm_gSthRightHandDL;

#define dmm_gSthRightForearmDL "__OTR__mm/objects/object_sth/gSthRightForearmDL"
static const ALIGN_ASSET(2) char mm_gSthRightForearmDL[] = dmm_gSthRightForearmDL;

#define dmm_gSthRightUpperArmDL "__OTR__mm/objects/object_sth/gSthRightUpperArmDL"
static const ALIGN_ASSET(2) char mm_gSthRightUpperArmDL[] = dmm_gSthRightUpperArmDL;

#define dmm_gSthLeftHandDL "__OTR__mm/objects/object_sth/gSthLeftHandDL"
static const ALIGN_ASSET(2) char mm_gSthLeftHandDL[] = dmm_gSthLeftHandDL;

#define dmm_gSthLeftForearmDL "__OTR__mm/objects/object_sth/gSthLeftForearmDL"
static const ALIGN_ASSET(2) char mm_gSthLeftForearmDL[] = dmm_gSthLeftForearmDL;

#define dmm_gSthLeftUpperArmDL "__OTR__mm/objects/object_sth/gSthLeftUpperArmDL"
static const ALIGN_ASSET(2) char mm_gSthLeftUpperArmDL[] = dmm_gSthLeftUpperArmDL;

#define dmm_gSthTorsoDL "__OTR__mm/objects/object_sth/gSthTorsoDL"
static const ALIGN_ASSET(2) char mm_gSthTorsoDL[] = dmm_gSthTorsoDL;

#define dmm_gSthRightFootDL "__OTR__mm/objects/object_sth/gSthRightFootDL"
static const ALIGN_ASSET(2) char mm_gSthRightFootDL[] = dmm_gSthRightFootDL;

#define dmm_gSthRightShinDL "__OTR__mm/objects/object_sth/gSthRightShinDL"
static const ALIGN_ASSET(2) char mm_gSthRightShinDL[] = dmm_gSthRightShinDL;

#define dmm_gSthRightThighDL "__OTR__mm/objects/object_sth/gSthRightThighDL"
static const ALIGN_ASSET(2) char mm_gSthRightThighDL[] = dmm_gSthRightThighDL;

#define dmm_gSthLeftFootDL "__OTR__mm/objects/object_sth/gSthLeftFootDL"
static const ALIGN_ASSET(2) char mm_gSthLeftFootDL[] = dmm_gSthLeftFootDL;

#define dmm_gSthLeftShinDL "__OTR__mm/objects/object_sth/gSthLeftShinDL"
static const ALIGN_ASSET(2) char mm_gSthLeftShinDL[] = dmm_gSthLeftShinDL;

#define dmm_gSthLeftThighDL "__OTR__mm/objects/object_sth/gSthLeftThighDL"
static const ALIGN_ASSET(2) char mm_gSthLeftThighDL[] = dmm_gSthLeftThighDL;

#define dmm_gSthPelvisDL "__OTR__mm/objects/object_sth/gSthPelvisDL"
static const ALIGN_ASSET(2) char mm_gSthPelvisDL[] = dmm_gSthPelvisDL;

#define dmm_gSthTLUT "__OTR__mm/objects/object_sth/gSthTLUT"
static const ALIGN_ASSET(2) char mm_gSthTLUT[] = dmm_gSthTLUT;

#define dmm_gSthSkinTex "__OTR__mm/objects/object_sth/gSthSkinTex"
static const ALIGN_ASSET(2) char mm_gSthSkinTex[] = dmm_gSthSkinTex;

#define dmm_gSthFingerTex "__OTR__mm/objects/object_sth/gSthFingerTex"
static const ALIGN_ASSET(2) char mm_gSthFingerTex[] = dmm_gSthFingerTex;

#define dmm_gSthShirtTex "__OTR__mm/objects/object_sth/gSthShirtTex"
static const ALIGN_ASSET(2) char mm_gSthShirtTex[] = dmm_gSthShirtTex;

#define dmm_gSthBeltBuckleTex "__OTR__mm/objects/object_sth/gSthBeltBuckleTex"
static const ALIGN_ASSET(2) char mm_gSthBeltBuckleTex[] = dmm_gSthBeltBuckleTex;

#define dmm_gSthPantsTex "__OTR__mm/objects/object_sth/gSthPantsTex"
static const ALIGN_ASSET(2) char mm_gSthPantsTex[] = dmm_gSthPantsTex;

#define dmm_gSthSkel "__OTR__mm/objects/object_sth/gSthSkel"
static const ALIGN_ASSET(2) char mm_gSthSkel[] = dmm_gSthSkel;

typedef enum ObjectSthLimb {
    /* 0x00 */ STH_LIMB_NONE,
    /* 0x01 */ STH_LIMB_PELVIS,
    /* 0x02 */ STH_LIMB_LEFT_THIGH,
    /* 0x03 */ STH_LIMB_LEFT_SHIN,
    /* 0x04 */ STH_LIMB_LEFT_FOOT,
    /* 0x05 */ STH_LIMB_RIGHT_THIGH,
    /* 0x06 */ STH_LIMB_RIGHT_SHIN,
    /* 0x07 */ STH_LIMB_RIGHT_FOOT,
    /* 0x08 */ STH_LIMB_CHEST,
    /* 0x09 */ STH_LIMB_LEFT_UPPER_ARM,
    /* 0x0A */ STH_LIMB_LEFT_FOREARM,
    /* 0x0B */ STH_LIMB_LEFT_HAND,
    /* 0x0C */ STH_LIMB_RIGHT_UPPER_ARM,
    /* 0x0D */ STH_LIMB_RIGHT_FOREARM,
    /* 0x0E */ STH_LIMB_RIGHT_HAND,
    /* 0x0F */ STH_LIMB_HEAD,
    /* 0x10 */ STH_LIMB_MAX
} ObjectSthLimb;

#endif // MM_OBJECTS_OBJECT_STH_H
