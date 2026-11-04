// MergeShip2 -- the Adult Mask in Hyrule (MergeShipAdultMask.h): its art, mounted at startup.
//
// The age change itself (MergeShip_AdultMask_SwitchAge) is in MergeShipAdultMask.c: it is plain C rebuilding the
// player in place, the same as MergeShipPlayerTransform.inc's MergeShip_ReinitAsForm, not the engine's C++ resource
// APIs this file exists for.

#include "MergeShipAdultMask.h"

#include <mergeship/AdultMask.hpp>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

static bool sArtMounted = false;
static bool sScreamMounted = false; // the mask as Link screams (MsAdultMask: MS_ADULT_MASK_SCREAM_DL)
static bool sChildMounted = false;  // the Child Mask (MsAdultMask: MS_CHILD_MASK_*)

extern "C" s32 MergeShip_AdultMask_ArtMounted(void) {
    return sArtMounted;
}

extern "C" s32 MergeShip_AdultMask_ScreamMounted(void) {
    return sArtMounted && sScreamMounted;
}

extern "C" s32 MergeShip_AdultMask_ChildMounted(void) {
    return sArtMounted && sChildMounted;
}

// Majora's Mask's resources are under mm/ here, Ocarina of Time's at the root.
extern "C" void MergeShip_AdultMask_Mount(void) {
    sArtMounted = MsAdultMask::Mount(Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager(), "mm/",
                                     "", &sScreamMounted, &sChildMounted);
}
