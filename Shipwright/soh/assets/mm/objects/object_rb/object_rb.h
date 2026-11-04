#ifndef MM_OBJECTS_OBJECT_RB_H
#define MM_OBJECTS_OBJECT_RB_H 1

#include "align_asset_macro.h"

#define dmm_gLeeverSpinAnim "__OTR__mm/objects/object_rb/gLeeverSpinAnim"
static const ALIGN_ASSET(2) char mm_gLeeverSpinAnim[] = dmm_gLeeverSpinAnim;

#define dmm_gLeeverBottomHalfDL "__OTR__mm/objects/object_rb/gLeeverBottomHalfDL"
static const ALIGN_ASSET(2) char mm_gLeeverBottomHalfDL[] = dmm_gLeeverBottomHalfDL;

#define dmm_gLeeverBottomHalfWrapper2DL "__OTR__mm/objects/object_rb/gLeeverBottomHalfWrapper2DL"
static const ALIGN_ASSET(2) char mm_gLeeverBottomHalfWrapper2DL[] = dmm_gLeeverBottomHalfWrapper2DL;

#define dmm_gLeeverBottomHalfWrapper1DL "__OTR__mm/objects/object_rb/gLeeverBottomHalfWrapper1DL"
static const ALIGN_ASSET(2) char mm_gLeeverBottomHalfWrapper1DL[] = dmm_gLeeverBottomHalfWrapper1DL;

#define dmm_gLeeverTopHalfDL "__OTR__mm/objects/object_rb/gLeeverTopHalfDL"
static const ALIGN_ASSET(2) char mm_gLeeverTopHalfDL[] = dmm_gLeeverTopHalfDL;

#define dmm_gLeeverTopHalfWrapper2DL "__OTR__mm/objects/object_rb/gLeeverTopHalfWrapper2DL"
static const ALIGN_ASSET(2) char mm_gLeeverTopHalfWrapper2DL[] = dmm_gLeeverTopHalfWrapper2DL;

#define dmm_gLeeverTopHalfWrapper1DL "__OTR__mm/objects/object_rb/gLeeverTopHalfWrapper1DL"
static const ALIGN_ASSET(2) char mm_gLeeverTopHalfWrapper1DL[] = dmm_gLeeverTopHalfWrapper1DL;

#define dmm_gLeeverSpike1DL "__OTR__mm/objects/object_rb/gLeeverSpike1DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike1DL[] = dmm_gLeeverSpike1DL;

#define dmm_gLeeverSpike1Wrapper2DL "__OTR__mm/objects/object_rb/gLeeverSpike1Wrapper2DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike1Wrapper2DL[] = dmm_gLeeverSpike1Wrapper2DL;

#define dmm_gLeeverSpike1Wrapper1DL "__OTR__mm/objects/object_rb/gLeeverSpike1Wrapper1DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike1Wrapper1DL[] = dmm_gLeeverSpike1Wrapper1DL;

#define dmm_gLeeverSpike2DL "__OTR__mm/objects/object_rb/gLeeverSpike2DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike2DL[] = dmm_gLeeverSpike2DL;

#define dmm_gLeeverSpike2Wrapper2DL "__OTR__mm/objects/object_rb/gLeeverSpike2Wrapper2DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike2Wrapper2DL[] = dmm_gLeeverSpike2Wrapper2DL;

#define dmm_gLeeverSpike2Wrapper1DL "__OTR__mm/objects/object_rb/gLeeverSpike2Wrapper1DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike2Wrapper1DL[] = dmm_gLeeverSpike2Wrapper1DL;

#define dmm_gLeeverSpike3DL "__OTR__mm/objects/object_rb/gLeeverSpike3DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike3DL[] = dmm_gLeeverSpike3DL;

#define dmm_gLeeverSpike3Wrapper2DL "__OTR__mm/objects/object_rb/gLeeverSpike3Wrapper2DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike3Wrapper2DL[] = dmm_gLeeverSpike3Wrapper2DL;

#define dmm_gLeeverSpike3Wrapper1DL "__OTR__mm/objects/object_rb/gLeeverSpike3Wrapper1DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike3Wrapper1DL[] = dmm_gLeeverSpike3Wrapper1DL;

#define dmm_gLeeverSpike4DL "__OTR__mm/objects/object_rb/gLeeverSpike4DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike4DL[] = dmm_gLeeverSpike4DL;

#define dmm_gLeeverSpike4Wrapper2DL "__OTR__mm/objects/object_rb/gLeeverSpike4Wrapper2DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike4Wrapper2DL[] = dmm_gLeeverSpike4Wrapper2DL;

#define dmm_gLeeverSpike4Wrapper1DL "__OTR__mm/objects/object_rb/gLeeverSpike4Wrapper1DL"
static const ALIGN_ASSET(2) char mm_gLeeverSpike4Wrapper1DL[] = dmm_gLeeverSpike4Wrapper1DL;

#define dmm_gLeeverSpikeTex "__OTR__mm/objects/object_rb/gLeeverSpikeTex"
static const ALIGN_ASSET(2) char mm_gLeeverSpikeTex[] = dmm_gLeeverSpikeTex;

#define dmm_gLeeverSideTex "__OTR__mm/objects/object_rb/gLeeverSideTex"
static const ALIGN_ASSET(2) char mm_gLeeverSideTex[] = dmm_gLeeverSideTex;

#define dmm_gLeeverFlowerTex "__OTR__mm/objects/object_rb/gLeeverFlowerTex"
static const ALIGN_ASSET(2) char mm_gLeeverFlowerTex[] = dmm_gLeeverFlowerTex;

#define dmm_gLeeverSkel "__OTR__mm/objects/object_rb/gLeeverSkel"
static const ALIGN_ASSET(2) char mm_gLeeverSkel[] = dmm_gLeeverSkel;

typedef enum LeeverLimb {
    /* 0x00 */ LEEVER_LIMB_NONE,
    /* 0x01 */ OBJECT_RB_LIMB_01,
    /* 0x02 */ OBJECT_RB_LIMB_02,
    /* 0x03 */ OBJECT_RB_LIMB_03,
    /* 0x04 */ LEEVER_LIMB_TOP_HALF,
    /* 0x05 */ OBJECT_RB_LIMB_05,
    /* 0x06 */ OBJECT_RB_LIMB_06,
    /* 0x07 */ LEEVER_LIMB_SPIKE_1,
    /* 0x08 */ OBJECT_RB_LIMB_08,
    /* 0x09 */ OBJECT_RB_LIMB_09,
    /* 0x0A */ LEEVER_LIMB_SPIKE_2,
    /* 0x0B */ OBJECT_RB_LIMB_0B,
    /* 0x0C */ OBJECT_RB_LIMB_0C,
    /* 0x0D */ LEEVER_LIMB_SPIKE_3,
    /* 0x0E */ OBJECT_RB_LIMB_0E,
    /* 0x0F */ OBJECT_RB_LIMB_0F,
    /* 0x10 */ LEEVER_LIMB_SPIKE_4,
    /* 0x11 */ LEEVER_LIMB_BOTTOM_HALF,
    /* 0x12 */ LEEVER_LIMB_MAX
} LeeverLimb;

#endif // MM_OBJECTS_OBJECT_RB_H
