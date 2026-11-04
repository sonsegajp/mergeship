#ifndef MM_OBJECTS_OBJECT_BOSS04_H
#define MM_OBJECTS_OBJECT_BOSS04_H 1

#include "align_asset_macro.h"

#define dmm_gWartIdleAnim "__OTR__mm/objects/object_boss04/gWartIdleAnim"
static const ALIGN_ASSET(2) char mm_gWartIdleAnim[] = dmm_gWartIdleAnim;

#define dmm_gWartShellDL "__OTR__mm/objects/object_boss04/gWartShellDL"
static const ALIGN_ASSET(2) char mm_gWartShellDL[] = dmm_gWartShellDL;

#define dmm_gWartBottomEyelidDL "__OTR__mm/objects/object_boss04/gWartBottomEyelidDL"
static const ALIGN_ASSET(2) char mm_gWartBottomEyelidDL[] = dmm_gWartBottomEyelidDL;

#define dmm_gWartTopEyelidDL "__OTR__mm/objects/object_boss04/gWartTopEyelidDL"
static const ALIGN_ASSET(2) char mm_gWartTopEyelidDL[] = dmm_gWartTopEyelidDL;

#define dmm_gWartEyeDL "__OTR__mm/objects/object_boss04/gWartEyeDL"
static const ALIGN_ASSET(2) char mm_gWartEyeDL[] = dmm_gWartEyeDL;

#define dmm_gWartShellTLUT "__OTR__mm/objects/object_boss04/gWartShellTLUT"
static const ALIGN_ASSET(2) char mm_gWartShellTLUT[] = dmm_gWartShellTLUT;

#define dmm_gWartRidgesTLUT "__OTR__mm/objects/object_boss04/gWartRidgesTLUT"
static const ALIGN_ASSET(2) char mm_gWartRidgesTLUT[] = dmm_gWartRidgesTLUT;

#define dmm_gWartShellTex "__OTR__mm/objects/object_boss04/gWartShellTex"
static const ALIGN_ASSET(2) char mm_gWartShellTex[] = dmm_gWartShellTex;

#define dmm_gWartRidgesTex "__OTR__mm/objects/object_boss04/gWartRidgesTex"
static const ALIGN_ASSET(2) char mm_gWartRidgesTex[] = dmm_gWartRidgesTex;

#define dmm_gWartEyeTex "__OTR__mm/objects/object_boss04/gWartEyeTex"
static const ALIGN_ASSET(2) char mm_gWartEyeTex[] = dmm_gWartEyeTex;

#define dmm_gWartBubbleOpaqueMaterialDL "__OTR__mm/objects/object_boss04/gWartBubbleOpaqueMaterialDL"
static const ALIGN_ASSET(2) char mm_gWartBubbleOpaqueMaterialDL[] = dmm_gWartBubbleOpaqueMaterialDL;

#define dmm_gWartBubbleMaterialDL "__OTR__mm/objects/object_boss04/gWartBubbleMaterialDL"
static const ALIGN_ASSET(2) char mm_gWartBubbleMaterialDL[] = dmm_gWartBubbleMaterialDL;

#define dmm_gWartBubbleModelDL "__OTR__mm/objects/object_boss04/gWartBubbleModelDL"
static const ALIGN_ASSET(2) char mm_gWartBubbleModelDL[] = dmm_gWartBubbleModelDL;

#define dmm_gWartBubbleTex "__OTR__mm/objects/object_boss04/gWartBubbleTex"
static const ALIGN_ASSET(2) char mm_gWartBubbleTex[] = dmm_gWartBubbleTex;

#define dmm_gWartShadowTex "__OTR__mm/objects/object_boss04/gWartShadowTex"
static const ALIGN_ASSET(2) char mm_gWartShadowTex[] = dmm_gWartShadowTex;

#define dmm_gWartShadowMaterialDL "__OTR__mm/objects/object_boss04/gWartShadowMaterialDL"
static const ALIGN_ASSET(2) char mm_gWartShadowMaterialDL[] = dmm_gWartShadowMaterialDL;

#define dmm_gWartShadowModelDL "__OTR__mm/objects/object_boss04/gWartShadowModelDL"
static const ALIGN_ASSET(2) char mm_gWartShadowModelDL[] = dmm_gWartShadowModelDL;

#define dmm_gWartSkel "__OTR__mm/objects/object_boss04/gWartSkel"
static const ALIGN_ASSET(2) char mm_gWartSkel[] = dmm_gWartSkel;

typedef enum WartLimb {
    /* 0x00 */ WART_LIMB_NONE,
    /* 0x01 */ WART_LIMB_ROOT,
    /* 0x02 */ WART_LIMB_BODY,
    /* 0x03 */ WART_LIMB_EYE_ROOT,
    /* 0x04 */ WART_LIMB_EYE,
    /* 0x05 */ WART_LIMB_TOP_EYELID_ROOT,
    /* 0x06 */ WART_LIMB_TOP_EYELID,
    /* 0x07 */ WART_LIMB_BOTTOM_EYELID_ROOT,
    /* 0x08 */ WART_LIMB_BOTTOM_EYELID,
    /* 0x09 */ WART_LIMB_MAX
} WartLimb;

#endif // MM_OBJECTS_OBJECT_BOSS04_H
