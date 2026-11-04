#ifndef MM_OBJECTS_OBJECT_FAMOS_H
#define MM_OBJECTS_OBJECT_FAMOS_H 1

#include "align_asset_macro.h"

#define dmm_gFamosShakeAnim "__OTR__mm/objects/object_famos/gFamosShakeAnim"
static const ALIGN_ASSET(2) char mm_gFamosShakeAnim[] = dmm_gFamosShakeAnim;

#define dmm_gFamosHelmetDL "__OTR__mm/objects/object_famos/gFamosHelmetDL"
static const ALIGN_ASSET(2) char mm_gFamosHelmetDL[] = dmm_gFamosHelmetDL;

#define dmm_gFamosMainBodyDL "__OTR__mm/objects/object_famos/gFamosMainBodyDL"
static const ALIGN_ASSET(2) char mm_gFamosMainBodyDL[] = dmm_gFamosMainBodyDL;

#define dmm_gFamosShieldDL "__OTR__mm/objects/object_famos/gFamosShieldDL"
static const ALIGN_ASSET(2) char mm_gFamosShieldDL[] = dmm_gFamosShieldDL;

#define dmm_gFamosSwordDL "__OTR__mm/objects/object_famos/gFamosSwordDL"
static const ALIGN_ASSET(2) char mm_gFamosSwordDL[] = dmm_gFamosSwordDL;

#define dmm_gFamosLightSwitchDL "__OTR__mm/objects/object_famos/gFamosLightSwitchDL"
static const ALIGN_ASSET(2) char mm_gFamosLightSwitchDL[] = dmm_gFamosLightSwitchDL;

#define dmm_gFamosBodyTLUT "__OTR__mm/objects/object_famos/gFamosBodyTLUT"
static const ALIGN_ASSET(2) char mm_gFamosBodyTLUT[] = dmm_gFamosBodyTLUT;

#define dmm_gFamosBottomTLUT "__OTR__mm/objects/object_famos/gFamosBottomTLUT"
static const ALIGN_ASSET(2) char mm_gFamosBottomTLUT[] = dmm_gFamosBottomTLUT;

#define dmm_gFamosShieldTLUT "__OTR__mm/objects/object_famos/gFamosShieldTLUT"
static const ALIGN_ASSET(2) char mm_gFamosShieldTLUT[] = dmm_gFamosShieldTLUT;

#define dmm_gFamosSwordHiltTLUT "__OTR__mm/objects/object_famos/gFamosSwordHiltTLUT"
static const ALIGN_ASSET(2) char mm_gFamosSwordHiltTLUT[] = dmm_gFamosSwordHiltTLUT;

#define dmm_gFamosMainBodyTex "__OTR__mm/objects/object_famos/gFamosMainBodyTex"
static const ALIGN_ASSET(2) char mm_gFamosMainBodyTex[] = dmm_gFamosMainBodyTex;

#define dmm_gFamosBottomTex "__OTR__mm/objects/object_famos/gFamosBottomTex"
static const ALIGN_ASSET(2) char mm_gFamosBottomTex[] = dmm_gFamosBottomTex;

#define dmm_gFamosShieldTex "__OTR__mm/objects/object_famos/gFamosShieldTex"
static const ALIGN_ASSET(2) char mm_gFamosShieldTex[] = dmm_gFamosShieldTex;

#define dmm_gFamosSwordHiltTex "__OTR__mm/objects/object_famos/gFamosSwordHiltTex"
static const ALIGN_ASSET(2) char mm_gFamosSwordHiltTex[] = dmm_gFamosSwordHiltTex;

#define dmm_gFamosLightSwitchTex "__OTR__mm/objects/object_famos/gFamosLightSwitchTex"
static const ALIGN_ASSET(2) char mm_gFamosLightSwitchTex[] = dmm_gFamosLightSwitchTex;

#define dmm_gFamosSkel "__OTR__mm/objects/object_famos/gFamosSkel"
static const ALIGN_ASSET(2) char mm_gFamosSkel[] = dmm_gFamosSkel;

#define dmm_gFamosIdleAnim "__OTR__mm/objects/object_famos/gFamosIdleAnim"
static const ALIGN_ASSET(2) char mm_gFamosIdleAnim[] = dmm_gFamosIdleAnim;

#define dmm_gFamosNormalGlowingEmblemTexAnim "__OTR__mm/objects/object_famos/gFamosNormalGlowingEmblemTexAnim"
static const ALIGN_ASSET(2) char mm_gFamosNormalGlowingEmblemTexAnim[] = dmm_gFamosNormalGlowingEmblemTexAnim;

#define dmm_gFamosFlippedGlowingEmblemTexAnim "__OTR__mm/objects/object_famos/gFamosFlippedGlowingEmblemTexAnim"
static const ALIGN_ASSET(2) char mm_gFamosFlippedGlowingEmblemTexAnim[] = dmm_gFamosFlippedGlowingEmblemTexAnim;

typedef enum FamosLimb {
    /* 0x00 */ FAMOS_LIMB_NONE,
    /* 0x01 */ FAMOS_LIMB_BODY,
    /* 0x02 */ FAMOS_LIMB_EMBLEM,
    /* 0x03 */ FAMOS_LIMB_SWORD,
    /* 0x04 */ FAMOS_LIMB_SHIELD,
    /* 0x05 */ FAMOS_LIMB_HEAD,
    /* 0x06 */ FAMOS_LIMB_MAX
} FamosLimb;


#endif // MM_OBJECTS_OBJECT_FAMOS_H
