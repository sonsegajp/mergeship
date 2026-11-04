// MergeShip2 -- registers the Majora's Mask actors the Zora form needs with ActorDB at boot. Each gets the
// next free actor id (ActorDBInit id -1), which the player reads from gMergeShipActorId_<Name> to spawn it.
// (Kept under soh/ because the build globs *.cpp only there; the actor itself is in
// src/overlays/actors/ovl_MM_En_Boom.)

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MM_En_Boom/z_mm_en_boom.h"
}

static void MergeShip_RegisterZoraActors() {
    if (gMergeShipActorId_EnBoom >= 0) {
        return;
    }

    // MM En_Boom_Profile: ACTORCAT_ITEMACTION, update and draw culling disabled, gameplay_keep.
    ActorDBInit entry = {
        "MM_En_Boom",
        "Zora Boomerang (Majora's Mask)",
        -1,
        ACTORCAT_ITEMACTION,
        (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED),
        OBJECT_GAMEPLAY_KEEP,
        sizeof(MMEnBoom),
        (ActorFunc)MMEnBoom_Init,
        (ActorFunc)MMEnBoom_Destroy,
        (ActorFunc)MMEnBoom_Update,
        (ActorFunc)MMEnBoom_Draw,
        nullptr,
    };
    gMergeShipActorId_EnBoom = (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

static RegisterShipInitFunc sMergeShipZoraActorsInit(MergeShip_RegisterZoraActors);
