#ifndef MM_OBJECTS_OBJECT_DEKUBABA_H
#define MM_OBJECTS_OBJECT_DEKUBABA_H 1

#include "align_asset_macro.h"

#define dmm_gDekuBabaPauseChompAnim "__OTR__mm/objects/object_dekubaba/gDekuBabaPauseChompAnim"
static const ALIGN_ASSET(2) char mm_gDekuBabaPauseChompAnim[] = dmm_gDekuBabaPauseChompAnim;

#define dmm_gDekuBabaFastChompAnim "__OTR__mm/objects/object_dekubaba/gDekuBabaFastChompAnim"
static const ALIGN_ASSET(2) char mm_gDekuBabaFastChompAnim[] = dmm_gDekuBabaFastChompAnim;

#define dmm_gDekuBabaLowerJawDL "__OTR__mm/objects/object_dekubaba/gDekuBabaLowerJawDL"
static const ALIGN_ASSET(2) char mm_gDekuBabaLowerJawDL[] = dmm_gDekuBabaLowerJawDL;

#define dmm_gDekuBabaUpperJawDL "__OTR__mm/objects/object_dekubaba/gDekuBabaUpperJawDL"
static const ALIGN_ASSET(2) char mm_gDekuBabaUpperJawDL[] = dmm_gDekuBabaUpperJawDL;

#define dmm_gDekuBabaBaseLeavesDL "__OTR__mm/objects/object_dekubaba/gDekuBabaBaseLeavesDL"
static const ALIGN_ASSET(2) char mm_gDekuBabaBaseLeavesDL[] = dmm_gDekuBabaBaseLeavesDL;

#define dmm_gDekuBabaStemTopDL "__OTR__mm/objects/object_dekubaba/gDekuBabaStemTopDL"
static const ALIGN_ASSET(2) char mm_gDekuBabaStemTopDL[] = dmm_gDekuBabaStemTopDL;

#define dmm_gDekuBabaStemMiddleDL "__OTR__mm/objects/object_dekubaba/gDekuBabaStemMiddleDL"
static const ALIGN_ASSET(2) char mm_gDekuBabaStemMiddleDL[] = dmm_gDekuBabaStemMiddleDL;

#define dmm_gDekuBabaStemBaseDL "__OTR__mm/objects/object_dekubaba/gDekuBabaStemBaseDL"
static const ALIGN_ASSET(2) char mm_gDekuBabaStemBaseDL[] = dmm_gDekuBabaStemBaseDL;

#define dmm_gDekuBabaJawOuterTex "__OTR__mm/objects/object_dekubaba/gDekuBabaJawOuterTex"
static const ALIGN_ASSET(2) char mm_gDekuBabaJawOuterTex[] = dmm_gDekuBabaJawOuterTex;

#define dmm_gDekuBabaJawInnerTex "__OTR__mm/objects/object_dekubaba/gDekuBabaJawInnerTex"
static const ALIGN_ASSET(2) char mm_gDekuBabaJawInnerTex[] = dmm_gDekuBabaJawInnerTex;

#define dmm_gDekuBabaLeafTex "__OTR__mm/objects/object_dekubaba/gDekuBabaLeafTex"
static const ALIGN_ASSET(2) char mm_gDekuBabaLeafTex[] = dmm_gDekuBabaLeafTex;

#define dmm_gDekuBabaBulbTex "__OTR__mm/objects/object_dekubaba/gDekuBabaBulbTex"
static const ALIGN_ASSET(2) char mm_gDekuBabaBulbTex[] = dmm_gDekuBabaBulbTex;

#define dmm_gDekuBabaStemTex "__OTR__mm/objects/object_dekubaba/gDekuBabaStemTex"
static const ALIGN_ASSET(2) char mm_gDekuBabaStemTex[] = dmm_gDekuBabaStemTex;

#define dmm_gDekuBabaSkel "__OTR__mm/objects/object_dekubaba/gDekuBabaSkel"
static const ALIGN_ASSET(2) char mm_gDekuBabaSkel[] = dmm_gDekuBabaSkel;

#define dmm_gDekuBabaStickDropDL "__OTR__mm/objects/object_dekubaba/gDekuBabaStickDropDL"
static const ALIGN_ASSET(2) char mm_gDekuBabaStickDropDL[] = dmm_gDekuBabaStickDropDL;

#define dmm_gDekuBabaStickDropTex "__OTR__mm/objects/object_dekubaba/gDekuBabaStickDropTex"
static const ALIGN_ASSET(2) char mm_gDekuBabaStickDropTex[] = dmm_gDekuBabaStickDropTex;

typedef enum DekuBabaLimb {
    /* 0x00 */ DEKUBABA_LIMB_NONE,
    /* 0x01 */ DEKUBABA_LIMB_ROOT,
    /* 0x02 */ DEKUBABA_LIMB_UPPER_JAW_ROOT,
    /* 0x03 */ DEKUBABA_LIMB_UPPER_JAW_WRAPPER,
    /* 0x04 */ DEKUBABA_LIMB_UPPER_JAW,
    /* 0x05 */ DEKUBABA_LIMB_LOWER_JAW_ROOT,
    /* 0x06 */ DEKUBABA_LIMB_LOWER_JAW_WRAPPER,
    /* 0x07 */ DEKUBABA_LIMB_LOWER_JAW,
    /* 0x08 */ DEKUBABA_LIMB_MAX
} DekuBabaLimb;

#endif // MM_OBJECTS_OBJECT_DEKUBABA_H
