#pragma once

// MergeShip2 -- Ocarina of Time's equipment assets, as resource paths in the OoT archive mounted under "oot/"
// (MergeShipAssets.h). Every one of them is Ocarina of Time's own (object_link_boy / object_link_child for the models on
// Link, object_gi_* for the get-item models, icon_item_static / item_name_static for the pause screen).

#include "align_asset_macro.h"
#include "2s2h/MergeShip/MergeShipAssets.h"

#define MS_OOTEQ_TEX(dir, name) OOT_ASSET("textures/" dir "/" name)
#define MS_OOTEQ_OBJ(obj, name) OOT_ASSET("objects/" obj "/" name)

// ---- icons (32x32 RGBA32) ---------------------------------------------------------------------------------------------
static const ALIGN_ASSET(2) char gOoTEqIconSwordMasterTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconSwordMasterTex");
static const ALIGN_ASSET(2) char gOoTEqIconSwordKokiriTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconSwordKokiriTex");
static const ALIGN_ASSET(2) char gOoTEqIconSwordBiggoronTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconSwordBiggoronTex");
static const ALIGN_ASSET(2) char gOoTEqIconBrokenKnifeTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconBrokenGiantsKnifeTex");
static const ALIGN_ASSET(2) char gOoTEqIconShieldDekuTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconShieldDekuTex");
static const ALIGN_ASSET(2) char gOoTEqIconShieldHylianTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconShieldHylianTex");
static const ALIGN_ASSET(2) char gOoTEqIconShieldMirrorTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconShieldMirrorTex");
static const ALIGN_ASSET(2) char gOoTEqIconTunicKokiriTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconTunicKokiriTex");
static const ALIGN_ASSET(2) char gOoTEqIconTunicGoronTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconTunicGoronTex");
static const ALIGN_ASSET(2) char gOoTEqIconTunicZoraTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconTunicZoraTex");
static const ALIGN_ASSET(2) char gOoTEqIconBootsKokiriTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconBootsKokiriTex");
static const ALIGN_ASSET(2) char gOoTEqIconBootsIronTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconBootsIronTex");
static const ALIGN_ASSET(2) char gOoTEqIconBootsHoverTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconBootsHoverTex");
static const ALIGN_ASSET(2) char gOoTEqIconBraceletTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconGoronsBraceletTex");
static const ALIGN_ASSET(2) char gOoTEqIconGauntletsSilverTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconSilverGauntletsTex");
static const ALIGN_ASSET(2) char gOoTEqIconGauntletsGoldTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconGoldenGauntletsTex");
static const ALIGN_ASSET(2) char gOoTEqIconScaleSilverTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconScaleSilverTex");
static const ALIGN_ASSET(2) char gOoTEqIconScaleGoldenTex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconScaleGoldenTex");
static const ALIGN_ASSET(2) char gOoTEqIconBulletBag30Tex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconBulletBag30Tex");
static const ALIGN_ASSET(2) char gOoTEqIconBulletBag40Tex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconBulletBag40Tex");
static const ALIGN_ASSET(2) char gOoTEqIconBulletBag50Tex[] = MS_OOTEQ_TEX("icon_item_static", "gItemIconBulletBag50Tex");
static const ALIGN_ASSET(2) char gOoTEqEquippedOutlineTex[] = MS_OOTEQ_TEX("parameter_static", "gEquippedItemOutlineTex");
