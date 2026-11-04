#pragma once

// MergeShip2 (soh_fd) -- the resources in mergeship2.o2r (packed from mergeship2/assets/o2r by GenerateMergeShip2Otr).
// None of them comes from a rom:
//   - the Deku pipes, Goron drums and Zora guitar ocarina item icons (32x32 RGBA32);
//   - Fierce Deity's ocarina meshes: Ocarina of Time's own Fairy Ocarina and Ocarina of Time geometry (their vertices
//     and textures are oot.o2r's, referenced by path), drawn in Fierce Deity's bare right hand;
//   - Adult Link's transformation scream, a WAV cut into Majora's Mask's voice drums (MergeShipSohFdAudio.cpp).

#include "align_asset_macro.h"

#define dgMergeShip2DekuPipesIconTex "__OTR__mergeship2/textures/gItemIconDekuPipesTex"
static const ALIGN_ASSET(2) char gMergeShip2DekuPipesIconTex[] = dgMergeShip2DekuPipesIconTex;

#define dgMergeShip2GoronDrumsIconTex "__OTR__mergeship2/textures/gItemIconGoronDrumsTex"
static const ALIGN_ASSET(2) char gMergeShip2GoronDrumsIconTex[] = dgMergeShip2GoronDrumsIconTex;

#define dgMergeShip2ZoraGuitarIconTex "__OTR__mergeship2/textures/gItemIconZoraGuitarTex"
static const ALIGN_ASSET(2) char gMergeShip2ZoraGuitarIconTex[] = dgMergeShip2ZoraGuitarIconTex;

#define dgMergeShip2FdFairyOcarinaDL "__OTR__mergeship2/objects/gFdFairyOcarinaDL"
static const ALIGN_ASSET(2) char gMergeShip2FdFairyOcarinaDL[] = dgMergeShip2FdFairyOcarinaDL;

#define dgMergeShip2FdOotOcarinaDL "__OTR__mergeship2/objects/gFdOotOcarinaDL"
static const ALIGN_ASSET(2) char gMergeShip2FdOotOcarinaDL[] = dgMergeShip2FdOotOcarinaDL;

// A raw archive file (not a resource), read by path.
#define MERGESHIP2_ADULT_SCREAM_WAV "mergeship2/audio/AdultTransformScream.wav"
