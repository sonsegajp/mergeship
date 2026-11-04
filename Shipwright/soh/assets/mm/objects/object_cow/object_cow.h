#ifndef MM_OBJECTS_OBJECT_COW_H
#define MM_OBJECTS_OBJECT_COW_H 1

#include "align_asset_macro.h"

#define dmm_gCowChewAnim "__OTR__mm/objects/object_cow/gCowChewAnim"
static const ALIGN_ASSET(2) char mm_gCowChewAnim[] = dmm_gCowChewAnim;

#define dmm_gCowBodyDL "__OTR__mm/objects/object_cow/gCowBodyDL"
static const ALIGN_ASSET(2) char mm_gCowBodyDL[] = dmm_gCowBodyDL;

#define dmm_gCowHeadDL "__OTR__mm/objects/object_cow/gCowHeadDL"
static const ALIGN_ASSET(2) char mm_gCowHeadDL[] = dmm_gCowHeadDL;

#define dmm_gCowNoseDL "__OTR__mm/objects/object_cow/gCowNoseDL"
static const ALIGN_ASSET(2) char mm_gCowNoseDL[] = dmm_gCowNoseDL;

#define dmm_gCowNoseRingDL "__OTR__mm/objects/object_cow/gCowNoseRingDL"
static const ALIGN_ASSET(2) char mm_gCowNoseRingDL[] = dmm_gCowNoseRingDL;

#define dmm_gCowJawDL "__OTR__mm/objects/object_cow/gCowJawDL"
static const ALIGN_ASSET(2) char mm_gCowJawDL[] = dmm_gCowJawDL;

#define dmm_gCowTLUT "__OTR__mm/objects/object_cow/gCowTLUT"
static const ALIGN_ASSET(2) char mm_gCowTLUT[] = dmm_gCowTLUT;

#define dmm_gCowUdderTex "__OTR__mm/objects/object_cow/gCowUdderTex"
static const ALIGN_ASSET(2) char mm_gCowUdderTex[] = dmm_gCowUdderTex;

#define dmm_gCowNoseRingTex "__OTR__mm/objects/object_cow/gCowNoseRingTex"
static const ALIGN_ASSET(2) char mm_gCowNoseRingTex[] = dmm_gCowNoseRingTex;

#define dmm_gCowNoseTex "__OTR__mm/objects/object_cow/gCowNoseTex"
static const ALIGN_ASSET(2) char mm_gCowNoseTex[] = dmm_gCowNoseTex;

#define dmm_gCowSpotsTex "__OTR__mm/objects/object_cow/gCowSpotsTex"
static const ALIGN_ASSET(2) char mm_gCowSpotsTex[] = dmm_gCowSpotsTex;

#define dmm_gCowEarTex "__OTR__mm/objects/object_cow/gCowEarTex"
static const ALIGN_ASSET(2) char mm_gCowEarTex[] = dmm_gCowEarTex;

#define dmm_gCowEyelidTex "__OTR__mm/objects/object_cow/gCowEyelidTex"
static const ALIGN_ASSET(2) char mm_gCowEyelidTex[] = dmm_gCowEyelidTex;

#define dmm_gCowLegAndTailTex "__OTR__mm/objects/object_cow/gCowLegAndTailTex"
static const ALIGN_ASSET(2) char mm_gCowLegAndTailTex[] = dmm_gCowLegAndTailTex;

#define dmm_gCowSkel "__OTR__mm/objects/object_cow/gCowSkel"
static const ALIGN_ASSET(2) char mm_gCowSkel[] = dmm_gCowSkel;

#define dmm_gCowMooAnim "__OTR__mm/objects/object_cow/gCowMooAnim"
static const ALIGN_ASSET(2) char mm_gCowMooAnim[] = dmm_gCowMooAnim;

#define dmm_gCowTailIdleAnim "__OTR__mm/objects/object_cow/gCowTailIdleAnim"
static const ALIGN_ASSET(2) char mm_gCowTailIdleAnim[] = dmm_gCowTailIdleAnim;

#define dmm_gCowTailBaseDL "__OTR__mm/objects/object_cow/gCowTailBaseDL"
static const ALIGN_ASSET(2) char mm_gCowTailBaseDL[] = dmm_gCowTailBaseDL;

#define dmm_gCowTailUpperDL "__OTR__mm/objects/object_cow/gCowTailUpperDL"
static const ALIGN_ASSET(2) char mm_gCowTailUpperDL[] = dmm_gCowTailUpperDL;

#define dmm_gCowTailMiddleDL "__OTR__mm/objects/object_cow/gCowTailMiddleDL"
static const ALIGN_ASSET(2) char mm_gCowTailMiddleDL[] = dmm_gCowTailMiddleDL;

#define dmm_gCowTailLowerDL "__OTR__mm/objects/object_cow/gCowTailLowerDL"
static const ALIGN_ASSET(2) char mm_gCowTailLowerDL[] = dmm_gCowTailLowerDL;

#define dmm_gCowTailEndDL "__OTR__mm/objects/object_cow/gCowTailEndDL"
static const ALIGN_ASSET(2) char mm_gCowTailEndDL[] = dmm_gCowTailEndDL;

#define dmm_gCowTailSkel "__OTR__mm/objects/object_cow/gCowTailSkel"
static const ALIGN_ASSET(2) char mm_gCowTailSkel[] = dmm_gCowTailSkel;

#define dmm_gCowTailSwishAnim "__OTR__mm/objects/object_cow/gCowTailSwishAnim"
static const ALIGN_ASSET(2) char mm_gCowTailSwishAnim[] = dmm_gCowTailSwishAnim;

typedef enum CowLimb {
    /* 0x00 */ COW_LIMB_NONE,
    /* 0x01 */ COW_LIMB_BODY,
    /* 0x02 */ COW_LIMB_HEAD,
    /* 0x03 */ COW_LIMB_JAW,
    /* 0x04 */ COW_LIMB_NOSE,
    /* 0x05 */ COW_LIMB_NOSE_RING,
    /* 0x06 */ COW_LIMB_MAX
} CowLimb;


typedef enum CowTailLimb {
    /* 0x00 */ COW_TAIL_LIMB_NONE,
    /* 0x01 */ COW_TAIL_LIMB_BASE,
    /* 0x02 */ COW_TAIL_LIMB_UPPER,
    /* 0x03 */ COW_TAIL_LIMB_MIDDLE,
    /* 0x04 */ COW_TAIL_LIMB_LOWER,
    /* 0x05 */ COW_TAIL_LIMB_END,
    /* 0x06 */ COW_TAIL_LIMB_MAX
} CowTailLimb;

#endif // MM_OBJECTS_OBJECT_COW_H
