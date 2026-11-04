// MergeShip2 -- registers Majora's Mask's En_M_Thunder (src/overlays/actors/ovl_MM_En_M_Thunder) with ActorDB
// at boot. Ocarina of Time's own En_M_Thunder keeps ACTOR_EN_M_THUNDER; this one takes the next free id,
// published as gMergeShipActorId_En_M_Thunder for the player to spawn (Fierce Deity's sword beam).
//
// This file lives under soh/soh/MergeShip rather than beside the actor because the soh target only globs
// .c/.h files under src/.

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MM_En_M_Thunder/z_mm_en_m_thunder.h"
}

static void MergeShip_RegisterMMEnMThunder() {
    if (gMergeShipActorId_En_M_Thunder >= 0) {
        return;
    }

    ActorDBInit entry;
    entry.name = "MM_En_M_Thunder";
    entry.desc = "Majora's Mask spin attack / sword beam";
    entry.category = ACTORCAT_ITEMACTION;
    entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = sizeof(MMEnMThunder);
    entry.init = (ActorFunc)MMEnMThunder_Init;
    entry.destroy = (ActorFunc)MMEnMThunder_Destroy;
    entry.update = (ActorFunc)MMEnMThunder_Update;
    entry.draw = (ActorFunc)MMEnMThunder_Draw;

    gMergeShipActorId_En_M_Thunder = (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

static RegisterShipInitFunc sMergeShipMMEnMThunderInit(MergeShip_RegisterMMEnMThunder);
