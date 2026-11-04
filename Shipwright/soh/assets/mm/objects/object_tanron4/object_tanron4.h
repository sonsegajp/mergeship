#ifndef MM_OBJECTS_OBJECT_TANRON4_H
#define MM_OBJECTS_OBJECT_TANRON4_H 1

#include "align_asset_macro.h"

#define dmm_gSeagullGlideAnim "__OTR__mm/objects/object_tanron4/gSeagullGlideAnim"
static const ALIGN_ASSET(2) char mm_gSeagullGlideAnim[] = dmm_gSeagullGlideAnim;

#define dmm_gSeagullFlapAnim "__OTR__mm/objects/object_tanron4/gSeagullFlapAnim"
static const ALIGN_ASSET(2) char mm_gSeagullFlapAnim[] = dmm_gSeagullFlapAnim;

#define dmm_gSeagullBodyDL "__OTR__mm/objects/object_tanron4/gSeagullBodyDL"
static const ALIGN_ASSET(2) char mm_gSeagullBodyDL[] = dmm_gSeagullBodyDL;

#define dmm_gSeagullRightWingEndDL "__OTR__mm/objects/object_tanron4/gSeagullRightWingEndDL"
static const ALIGN_ASSET(2) char mm_gSeagullRightWingEndDL[] = dmm_gSeagullRightWingEndDL;

#define dmm_gSeagullRightWingStartDL "__OTR__mm/objects/object_tanron4/gSeagullRightWingStartDL"
static const ALIGN_ASSET(2) char mm_gSeagullRightWingStartDL[] = dmm_gSeagullRightWingStartDL;

#define dmm_gSeagullLeftWingEndDL "__OTR__mm/objects/object_tanron4/gSeagullLeftWingEndDL"
static const ALIGN_ASSET(2) char mm_gSeagullLeftWingEndDL[] = dmm_gSeagullLeftWingEndDL;

#define dmm_gSeagullLeftWingStartDL "__OTR__mm/objects/object_tanron4/gSeagullLeftWingStartDL"
static const ALIGN_ASSET(2) char mm_gSeagullLeftWingStartDL[] = dmm_gSeagullLeftWingStartDL;

#define dmm_gSeagullBodyTex "__OTR__mm/objects/object_tanron4/gSeagullBodyTex"
static const ALIGN_ASSET(2) char mm_gSeagullBodyTex[] = dmm_gSeagullBodyTex;

#define dmm_gSeagullWingTex "__OTR__mm/objects/object_tanron4/gSeagullWingTex"
static const ALIGN_ASSET(2) char mm_gSeagullWingTex[] = dmm_gSeagullWingTex;

#define dmm_gSeagullSkel "__OTR__mm/objects/object_tanron4/gSeagullSkel"
static const ALIGN_ASSET(2) char mm_gSeagullSkel[] = dmm_gSeagullSkel;

typedef enum ObjectTanron4Limb {
    /* 0x00 */ SEAGULL_LIMB_RIGHT_WING_NONE,
    /* 0x01 */ SEAGULL_LIMB_ROOT,
    /* 0x02 */ SEAGULL_LIMB_BODY,
    /* 0x03 */ SEAGULL_LIMB_LEFT_WING_ROOT,
    /* 0x04 */ SEAGULL_LIMB_LEFT_WING_WRAPPER,
    /* 0x05 */ SEAGULL_LIMB_LEFT_WING_START,
    /* 0x06 */ SEAGULL_LIMB_LEFT_WING_END,
    /* 0x07 */ SEAGULL_LIMB_RIGHT_WING_ROOT,
    /* 0x08 */ SEAGULL_LIMB_RIGHT_WING_WRAPPER,
    /* 0x09 */ SEAGULL_LIMB_RIGHT_WING_START,
    /* 0x0A */ SEAGULL_LIMB_RIGHT_WING_END,
    /* 0x0B */ SEAGULL_LIMB_RIGHT_WING_MAX
} ObjectTanron4Limb;

#endif // MM_OBJECTS_OBJECT_TANRON4_H
