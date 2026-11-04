#ifndef MM_OBJECTS_OBJECT_PO_FUSEN_H
#define MM_OBJECTS_OBJECT_PO_FUSEN_H 1

#include "align_asset_macro.h"

#define dmm_gPoeBalloonEmptyAnim "__OTR__mm/objects/object_po_fusen/gPoeBalloonEmptyAnim"
static const ALIGN_ASSET(2) char mm_gPoeBalloonEmptyAnim[] = dmm_gPoeBalloonEmptyAnim;

#define dmm_gPoeBalloonChainAndLanternDL "__OTR__mm/objects/object_po_fusen/gPoeBalloonChainAndLanternDL"
static const ALIGN_ASSET(2) char mm_gPoeBalloonChainAndLanternDL[] = dmm_gPoeBalloonChainAndLanternDL;

#define dmm_gPoeBalloonLeftHandDL "__OTR__mm/objects/object_po_fusen/gPoeBalloonLeftHandDL"
static const ALIGN_ASSET(2) char mm_gPoeBalloonLeftHandDL[] = dmm_gPoeBalloonLeftHandDL;

#define dmm_gPoeBalloonLeftForearmDL "__OTR__mm/objects/object_po_fusen/gPoeBalloonLeftForearmDL"
static const ALIGN_ASSET(2) char mm_gPoeBalloonLeftForearmDL[] = dmm_gPoeBalloonLeftForearmDL;

#define dmm_gPoeBalloonLeftUpperArmDL "__OTR__mm/objects/object_po_fusen/gPoeBalloonLeftUpperArmDL"
static const ALIGN_ASSET(2) char mm_gPoeBalloonLeftUpperArmDL[] = dmm_gPoeBalloonLeftUpperArmDL;

#define dmm_gPoeBalloonRightHandDL "__OTR__mm/objects/object_po_fusen/gPoeBalloonRightHandDL"
static const ALIGN_ASSET(2) char mm_gPoeBalloonRightHandDL[] = dmm_gPoeBalloonRightHandDL;

#define dmm_gPoeBalloonRightForearmDL "__OTR__mm/objects/object_po_fusen/gPoeBalloonRightForearmDL"
static const ALIGN_ASSET(2) char mm_gPoeBalloonRightForearmDL[] = dmm_gPoeBalloonRightForearmDL;

#define dmm_gPoeBalloonRightUpperArmDL "__OTR__mm/objects/object_po_fusen/gPoeBalloonRightUpperArmDL"
static const ALIGN_ASSET(2) char mm_gPoeBalloonRightUpperArmDL[] = dmm_gPoeBalloonRightUpperArmDL;

#define dmm_gPoeBalloonBodyDL "__OTR__mm/objects/object_po_fusen/gPoeBalloonBodyDL"
static const ALIGN_ASSET(2) char mm_gPoeBalloonBodyDL[] = dmm_gPoeBalloonBodyDL;

#define dmm_gPoeBalloonTLUT "__OTR__mm/objects/object_po_fusen/gPoeBalloonTLUT"
static const ALIGN_ASSET(2) char mm_gPoeBalloonTLUT[] = dmm_gPoeBalloonTLUT;

#define dmm_gPoeBalloonTatteredChinTex "__OTR__mm/objects/object_po_fusen/gPoeBalloonTatteredChinTex"
static const ALIGN_ASSET(2) char mm_gPoeBalloonTatteredChinTex[] = dmm_gPoeBalloonTatteredChinTex;

#define dmm_gPoeBalloonLanternTopTex "__OTR__mm/objects/object_po_fusen/gPoeBalloonLanternTopTex"
static const ALIGN_ASSET(2) char mm_gPoeBalloonLanternTopTex[] = dmm_gPoeBalloonLanternTopTex;

#define dmm_gPoeBalloonLanternStandTex "__OTR__mm/objects/object_po_fusen/gPoeBalloonLanternStandTex"
static const ALIGN_ASSET(2) char mm_gPoeBalloonLanternStandTex[] = dmm_gPoeBalloonLanternStandTex;

#define dmm_gPoeBalloonLanternGlassTex "__OTR__mm/objects/object_po_fusen/gPoeBalloonLanternGlassTex"
static const ALIGN_ASSET(2) char mm_gPoeBalloonLanternGlassTex[] = dmm_gPoeBalloonLanternGlassTex;

#define dmm_gPoeBalloonHandTex "__OTR__mm/objects/object_po_fusen/gPoeBalloonHandTex"
static const ALIGN_ASSET(2) char mm_gPoeBalloonHandTex[] = dmm_gPoeBalloonHandTex;

#define dmm_gPoeBalloonOrangeSkinTex "__OTR__mm/objects/object_po_fusen/gPoeBalloonOrangeSkinTex"
static const ALIGN_ASSET(2) char mm_gPoeBalloonOrangeSkinTex[] = dmm_gPoeBalloonOrangeSkinTex;

#define dmm_gPoeBalloonFaceTex "__OTR__mm/objects/object_po_fusen/gPoeBalloonFaceTex"
static const ALIGN_ASSET(2) char mm_gPoeBalloonFaceTex[] = dmm_gPoeBalloonFaceTex;

#define dmm_gPoeBalloonChainLinkTex "__OTR__mm/objects/object_po_fusen/gPoeBalloonChainLinkTex"
static const ALIGN_ASSET(2) char mm_gPoeBalloonChainLinkTex[] = dmm_gPoeBalloonChainLinkTex;

#define dmm_gPoeBalloonSkel "__OTR__mm/objects/object_po_fusen/gPoeBalloonSkel"
static const ALIGN_ASSET(2) char mm_gPoeBalloonSkel[] = dmm_gPoeBalloonSkel;

typedef enum PoeBalloonLimb {
    /* 0x00 */ POE_BALLOON_LIMB_NONE,
    /* 0x01 */ POE_BALLOON_LIMB_ROOT,
    /* 0x02 */ POE_BALLOON_LIMB_BODY,
    /* 0x03 */ POE_BALLOON_RIGHT_UPPER_ARM,
    /* 0x04 */ POE_BALLOON_RIGHT_FOREARM,
    /* 0x05 */ POE_BALLOON_RIGHT_HAND,
    /* 0x06 */ POE_BALLOON_LEFT_UPPER_ARM,
    /* 0x07 */ POE_BALLOON_LEFT_FOREARM,
    /* 0x08 */ POE_BALLOON_LEFT_HAND,
    /* 0x09 */ POE_BALLOON_LIMB_CHAIN_AND_LANTERN,
    /* 0x0A */ POE_BALLOON_LIMB_MAX
} PoeBalloonLimb;

#endif // MM_OBJECTS_OBJECT_PO_FUSEN_H
