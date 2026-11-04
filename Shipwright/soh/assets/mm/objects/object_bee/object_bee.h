#ifndef MM_OBJECTS_OBJECT_BEE_H
#define MM_OBJECTS_OBJECT_BEE_H 1

#include "align_asset_macro.h"

#define dmm_gBeeFlyingAnim "__OTR__mm/objects/object_bee/gBeeFlyingAnim"
static const ALIGN_ASSET(2) char mm_gBeeFlyingAnim[] = dmm_gBeeFlyingAnim;

#define dmm_gBeeBodyDL "__OTR__mm/objects/object_bee/gBeeBodyDL"
static const ALIGN_ASSET(2) char mm_gBeeBodyDL[] = dmm_gBeeBodyDL;

#define dmm_gBeeHeadDL "__OTR__mm/objects/object_bee/gBeeHeadDL"
static const ALIGN_ASSET(2) char mm_gBeeHeadDL[] = dmm_gBeeHeadDL;

#define dmm_gBeeAntennaeDL "__OTR__mm/objects/object_bee/gBeeAntennaeDL"
static const ALIGN_ASSET(2) char mm_gBeeAntennaeDL[] = dmm_gBeeAntennaeDL;

#define dmm_gBeeRightWingDL "__OTR__mm/objects/object_bee/gBeeRightWingDL"
static const ALIGN_ASSET(2) char mm_gBeeRightWingDL[] = dmm_gBeeRightWingDL;

#define dmm_gBeeLeftWingDL "__OTR__mm/objects/object_bee/gBeeLeftWingDL"
static const ALIGN_ASSET(2) char mm_gBeeLeftWingDL[] = dmm_gBeeLeftWingDL;

#define dmm_gBeeWingTLUT "__OTR__mm/objects/object_bee/gBeeWingTLUT"
static const ALIGN_ASSET(2) char mm_gBeeWingTLUT[] = dmm_gBeeWingTLUT;

#define dmm_gBeeHeadTLUT "__OTR__mm/objects/object_bee/gBeeHeadTLUT"
static const ALIGN_ASSET(2) char mm_gBeeHeadTLUT[] = dmm_gBeeHeadTLUT;

#define dmm_gBeeBodyTLUT "__OTR__mm/objects/object_bee/gBeeBodyTLUT"
static const ALIGN_ASSET(2) char mm_gBeeBodyTLUT[] = dmm_gBeeBodyTLUT;

#define dmm_gBeeAntennaeTLUT "__OTR__mm/objects/object_bee/gBeeAntennaeTLUT"
static const ALIGN_ASSET(2) char mm_gBeeAntennaeTLUT[] = dmm_gBeeAntennaeTLUT;

#define dmm_gBeeWingTex "__OTR__mm/objects/object_bee/gBeeWingTex"
static const ALIGN_ASSET(2) char mm_gBeeWingTex[] = dmm_gBeeWingTex;

#define dmm_gBeeHeadTex "__OTR__mm/objects/object_bee/gBeeHeadTex"
static const ALIGN_ASSET(2) char mm_gBeeHeadTex[] = dmm_gBeeHeadTex;

#define dmm_gBeeBodyTex "__OTR__mm/objects/object_bee/gBeeBodyTex"
static const ALIGN_ASSET(2) char mm_gBeeBodyTex[] = dmm_gBeeBodyTex;

#define dmm_gBeeAntennaeTex "__OTR__mm/objects/object_bee/gBeeAntennaeTex"
static const ALIGN_ASSET(2) char mm_gBeeAntennaeTex[] = dmm_gBeeAntennaeTex;

#define dmm_gBeeSkel "__OTR__mm/objects/object_bee/gBeeSkel"
static const ALIGN_ASSET(2) char mm_gBeeSkel[] = dmm_gBeeSkel;

typedef enum ObjectBeeLimb {
    /* 0x00 */ OBJECT_BEE_LIMB_NONE,
    /* 0x01 */ OBJECT_BEE_LIMB_ROOT,
    /* 0x02 */ OBJECT_BEE_LIMB_WINGS_ROOT,
    /* 0x03 */ OBJECT_BEE_LIMB_LEFT_WING_ROOT,
    /* 0x04 */ OBJECT_BEE_LIMB_LEFT_WING,
    /* 0x05 */ OBJECT_BEE_LIMB_RIGHT_WING_ROOT,
    /* 0x06 */ OBJECT_BEE_LIMB_RIGHT_WING,
    /* 0x07 */ OBJECT_BEE_LIMB_BODY,
    /* 0x08 */ OBJECT_BEE_LIMB_ANTENNAE,
    /* 0x09 */ OBJECT_BEE_LIMB_HEAD,
    /* 0x0A */ OBJECT_BEE_LIMB_MAX
} ObjectBeeLimb;

#endif // MM_OBJECTS_OBJECT_BEE_H
