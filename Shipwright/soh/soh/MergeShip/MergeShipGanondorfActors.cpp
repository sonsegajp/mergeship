// MergeShip2 -- run-time registration of Ganondorf mode's magic actor (src/overlays/actors/ovl_MS_Ganon_Magic), the
// same way as the Deku port's (forms/MergeShipDekuActors.cpp).

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MS_Ganon_Magic/z_ms_ganon_magic.h"
}

static void MergeShip_RegisterGanondorfActors() {
    static bool sRegistered = false;

    if (sRegistered || (ActorDB::Instance == nullptr)) {
        return;
    }
    sRegistered = true;

    ActorDBInit magic = {
        "MS_Ganon_Magic",
        "Ganondorf mode: the bosses' magic",
        -1,
        ACTORCAT_ITEMACTION,
        (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED),
        OBJECT_GAMEPLAY_KEEP,
        sizeof(MsGanonMagic),
        (ActorFunc)MsGanonMagic_Init,
        (ActorFunc)MsGanonMagic_Destroy,
        (ActorFunc)MsGanonMagic_Update,
        (ActorFunc)MsGanonMagic_Draw,
        nullptr,
    };
    ActorDB::Instance->AddEntry(magic);
    gMergeShipActorId_MS_Ganon_Magic = (s16)ActorDB::Instance->RetrieveId("MS_Ganon_Magic");
}

static RegisterShipInitFunc sMergeShipGanondorfActorsInit(MergeShip_RegisterGanondorfActors);

extern "C" void MergeShip_Ganondorf_EnsureActorsRegistered(void) {
    MergeShip_RegisterGanondorfActors();
}
