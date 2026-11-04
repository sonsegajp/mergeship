#ifndef MM_OBJECTS_OBJECT_VM_H
#define MM_OBJECTS_OBJECT_VM_H 1

#include "align_asset_macro.h"

#define dmm_gBeamosAnim "__OTR__mm/objects/object_vm/gBeamosAnim"
static const ALIGN_ASSET(2) char mm_gBeamosAnim[] = dmm_gBeamosAnim;

#define dmm_gBeamosBodyDL "__OTR__mm/objects/object_vm/gBeamosBodyDL"
static const ALIGN_ASSET(2) char mm_gBeamosBodyDL[] = dmm_gBeamosBodyDL;

#define dmm_gBeamosTopEyelidDL "__OTR__mm/objects/object_vm/gBeamosTopEyelidDL"
static const ALIGN_ASSET(2) char mm_gBeamosTopEyelidDL[] = dmm_gBeamosTopEyelidDL;

#define dmm_gBeamosBottomEyelidDL "__OTR__mm/objects/object_vm/gBeamosBottomEyelidDL"
static const ALIGN_ASSET(2) char mm_gBeamosBottomEyelidDL[] = dmm_gBeamosBottomEyelidDL;

#define dmm_gBeamosEyeDL "__OTR__mm/objects/object_vm/gBeamosEyeDL"
static const ALIGN_ASSET(2) char mm_gBeamosEyeDL[] = dmm_gBeamosEyeDL;

#define dmm_gBeamosLaserDL "__OTR__mm/objects/object_vm/gBeamosLaserDL"
static const ALIGN_ASSET(2) char mm_gBeamosLaserDL[] = dmm_gBeamosLaserDL;

#define dmm_gBeamosBodyGradientTex "__OTR__mm/objects/object_vm/gBeamosBodyGradientTex"
static const ALIGN_ASSET(2) char mm_gBeamosBodyGradientTex[] = dmm_gBeamosBodyGradientTex;

#define dmm_gBeamosGrayMetalTex "__OTR__mm/objects/object_vm/gBeamosGrayMetalTex"
static const ALIGN_ASSET(2) char mm_gBeamosGrayMetalTex[] = dmm_gBeamosGrayMetalTex;

#define dmm_gBeamosBoltAndMetalTex "__OTR__mm/objects/object_vm/gBeamosBoltAndMetalTex"
static const ALIGN_ASSET(2) char mm_gBeamosBoltAndMetalTex[] = dmm_gBeamosBoltAndMetalTex;

#define dmm_gBeamosInnerEyeLidTex "__OTR__mm/objects/object_vm/gBeamosInnerEyeLidTex"
static const ALIGN_ASSET(2) char mm_gBeamosInnerEyeLidTex[] = dmm_gBeamosInnerEyeLidTex;

#define dmm_gBeamosEyeOutlineTex "__OTR__mm/objects/object_vm/gBeamosEyeOutlineTex"
static const ALIGN_ASSET(2) char mm_gBeamosEyeOutlineTex[] = dmm_gBeamosEyeOutlineTex;

#define dmm_gBeamosEyeTex "__OTR__mm/objects/object_vm/gBeamosEyeTex"
static const ALIGN_ASSET(2) char mm_gBeamosEyeTex[] = dmm_gBeamosEyeTex;

#define dmm_gBeamosBodyTex "__OTR__mm/objects/object_vm/gBeamosBodyTex"
static const ALIGN_ASSET(2) char mm_gBeamosBodyTex[] = dmm_gBeamosBodyTex;

#define dmm_gBeamosLaserTex "__OTR__mm/objects/object_vm/gBeamosLaserTex"
static const ALIGN_ASSET(2) char mm_gBeamosLaserTex[] = dmm_gBeamosLaserTex;

#define dmm_gBeamosTeethTex "__OTR__mm/objects/object_vm/gBeamosTeethTex"
static const ALIGN_ASSET(2) char mm_gBeamosTeethTex[] = dmm_gBeamosTeethTex;

#define dmm_gBeamosSkel "__OTR__mm/objects/object_vm/gBeamosSkel"
static const ALIGN_ASSET(2) char mm_gBeamosSkel[] = dmm_gBeamosSkel;

typedef enum BeamosLimb {
    /* 0x00 */ BEAMOS_LIMB_NONE,
    /* 0x01 */ BEAMOS_LIMB_ROOT,
    /* 0x02 */ BEAMOS_LIMB_HEAD_ROOT,
    /* 0x03 */ BEAMOS_LIMB_TOP_EYELID_ROOT,
    /* 0x04 */ BEAMOS_LIMB_TOP_EYELID_WRAPPER,
    /* 0x05 */ BEAMOS_LIMB_TOP_EYELID,
    /* 0x06 */ BEAMOS_LIMB_BOTTOM_EYELID_ROOT,
    /* 0x07 */ BEAMOS_LIMB_BOTTOM_EYELID_WRAPPER,
    /* 0x08 */ BEAMOS_LIMB_BOTTEM_EYELID,
    /* 0x09 */ BEAMOS_LIMB_EYE,
    /* 0x0A */ BEAMOS_LIMB_BODY,
    /* 0x0B */ BEAMOS_LIMB_MAX
} BeamosLimb;

#endif // MM_OBJECTS_OBJECT_VM_H
