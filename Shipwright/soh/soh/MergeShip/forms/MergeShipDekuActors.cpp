// MergeShip2 -- run-time registration of the Majora's Mask actors the Deku port needs (PORTING_GUIDE rule 5).
//
// The actor sources live in src/overlays/actors/ovl_MM_*; this file sits here rather than next to them because
// the soh target only globs *.c under src/. Each actor takes the next free ActorDB id; the player spawns them by
// the ids published below.

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MM_En_Arrow/z_mm_en_arrow.h"
#include "src/overlays/actors/ovl_MM_Obj_Etcetera/z_mm_obj_etcetera.h"
}

static void MergeShip_RegisterDekuActors() {
    static bool sRegistered = false;

    if (sRegistered || (ActorDB::Instance == nullptr)) {
        return;
    }
    sRegistered = true;

    // MM En_Arrow_Profile: ACTORCAT_ITEMACTION, UPDATE/DRAW culling disabled, GAMEPLAY_KEEP.
    ActorDBInit arrow = {
        "MM_En_Arrow",
        "Majora's Mask projectile (Deku bubble, Deku nut)",
        -1,
        ACTORCAT_ITEMACTION,
        (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED),
        OBJECT_GAMEPLAY_KEEP,
        sizeof(MMEnArrow),
        (ActorFunc)MMEnArrow_Init,
        (ActorFunc)MMEnArrow_Destroy,
        (ActorFunc)MMEnArrow_Update,
        (ActorFunc)MMEnArrow_Draw,
        nullptr,
    };
    ActorDB::Instance->AddEntry(arrow);
    gMergeShipActorId_MM_En_Arrow = (s16)ActorDB::Instance->RetrieveId("MM_En_Arrow");

    // MM Obj_Etcetera_Profile: ACTORCAT_BG, UPDATE culling disabled, GAMEPLAY_KEEP, no draw until set up.
    ActorDBInit flower = {
        "MM_Obj_Etcetera",
        "Majora's Mask Deku Flower",
        -1,
        ACTORCAT_BG,
        ACTOR_FLAG_UPDATE_CULLING_DISABLED,
        OBJECT_GAMEPLAY_KEEP,
        sizeof(MMObjEtcetera),
        (ActorFunc)MMObjEtcetera_Init,
        (ActorFunc)MMObjEtcetera_Destroy,
        (ActorFunc)MMObjEtcetera_Update,
        nullptr,
        nullptr,
    };
    ActorDB::Instance->AddEntry(flower);
    gMergeShipActorId_MM_Obj_Etcetera = (s16)ActorDB::Instance->RetrieveId("MM_Obj_Etcetera");
}

static RegisterShipInitFunc sMergeShipDekuActorsInit(MergeShip_RegisterDekuActors);

// For the player: registers the actors if boot-time registration has not happened yet.
extern "C" void MergeShip_Deku_EnsureActorsRegistered(void) {
    MergeShip_RegisterDekuActors();
}
