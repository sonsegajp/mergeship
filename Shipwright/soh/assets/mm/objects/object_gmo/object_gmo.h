#ifndef MM_OBJECTS_OBJECT_GMO_H
#define MM_OBJECTS_OBJECT_GMO_H 1

#include "align_asset_macro.h"

#define dmm_gNejironIdleAnim "__OTR__mm/objects/object_gmo/gNejironIdleAnim"
static const ALIGN_ASSET(2) char mm_gNejironIdleAnim[] = dmm_gNejironIdleAnim;

#define dmm_gNejironBodyDL "__OTR__mm/objects/object_gmo/gNejironBodyDL"
static const ALIGN_ASSET(2) char mm_gNejironBodyDL[] = dmm_gNejironBodyDL;

#define dmm_gNejironEyesDL "__OTR__mm/objects/object_gmo/gNejironEyesDL"
static const ALIGN_ASSET(2) char mm_gNejironEyesDL[] = dmm_gNejironEyesDL;

#define dmm_gNejironBodyTLUT "__OTR__mm/objects/object_gmo/gNejironBodyTLUT"
static const ALIGN_ASSET(2) char mm_gNejironBodyTLUT[] = dmm_gNejironBodyTLUT;

#define dmm_gNejironBodyTex "__OTR__mm/objects/object_gmo/gNejironBodyTex"
static const ALIGN_ASSET(2) char mm_gNejironBodyTex[] = dmm_gNejironBodyTex;

#define dmm_gNejironEyeOpenTex "__OTR__mm/objects/object_gmo/gNejironEyeOpenTex"
static const ALIGN_ASSET(2) char mm_gNejironEyeOpenTex[] = dmm_gNejironEyeOpenTex;

#define dmm_gNejironEyeHalfTex "__OTR__mm/objects/object_gmo/gNejironEyeHalfTex"
static const ALIGN_ASSET(2) char mm_gNejironEyeHalfTex[] = dmm_gNejironEyeHalfTex;

#define dmm_gNejironEyeClosedTex "__OTR__mm/objects/object_gmo/gNejironEyeClosedTex"
static const ALIGN_ASSET(2) char mm_gNejironEyeClosedTex[] = dmm_gNejironEyeClosedTex;

#define dmm_gNejironSkel "__OTR__mm/objects/object_gmo/gNejironSkel"
static const ALIGN_ASSET(2) char mm_gNejironSkel[] = dmm_gNejironSkel;

typedef enum NejironLimb {
    /* 0x00 */ NEJIRON_LIMB_NONE,
    /* 0x01 */ NEJIRON_LIMB_BODY,
    /* 0x02 */ NEJIRON_LIMB_EYES,
    /* 0x03 */ NEJIRON_LIMB_MAX
} NejironLimb;

#endif // MM_OBJECTS_OBJECT_GMO_H
