// MergeShip2 -- registers Majora's Mask's powder keg (MM En_Bom's powder keg branch,
// src/overlays/actors/ovl_MM_En_Bom_Keg) with ActorDB at boot. Ocarina of Time's own En_Bom keeps ACTOR_EN_BOM for
// bombs; the keg takes the next free id, published as gMergeShipActorId_MM_En_Bom_Keg for the player to spawn
// (MM Player_InitExplosiveIA, the Goron's Powder Keg).
//
// This file lives under soh/soh/MergeShip rather than beside the actor because the soh target only globs
// .c/.h files under src/.

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MM_En_Bom_Keg/z_mm_en_bom_keg.h"
}

static void MergeShip_RegisterMMEnBomKeg() {
    if (gMergeShipActorId_MM_En_Bom_Keg >= 0) {
        return;
    }

    ActorDBInit entry;
    entry.name = "MM_En_Bom_Keg";
    entry.desc = "Majora's Mask powder keg";
    entry.category = ACTORCAT_EXPLOSIVE;
    entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = sizeof(MMEnBomKeg);
    entry.init = (ActorFunc)MMEnBomKeg_Init;
    entry.destroy = (ActorFunc)MMEnBomKeg_Destroy;
    entry.update = (ActorFunc)MMEnBomKeg_Update;
    entry.draw = (ActorFunc)MMEnBomKeg_Draw;

    gMergeShipActorId_MM_En_Bom_Keg = (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

static RegisterShipInitFunc sMergeShipMMEnBomKegInit(MergeShip_RegisterMMEnBomKeg);
