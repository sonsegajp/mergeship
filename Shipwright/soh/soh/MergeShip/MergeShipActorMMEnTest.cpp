// MergeShip2 -- registers Majora's Mask's En_Test (src/overlays/actors/ovl_MM_En_Test), the crater marks, with
// ActorDB at boot. Ocarina of Time has no En_Test; this one takes the next free id, published as
// gMergeShipActorId_En_Test for the player to spawn (Goron Link's punch and ground pound).
//
// This file lives under soh/soh/MergeShip rather than beside the actor because the soh target only globs
// .c/.h files under src/.

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MM_En_Test/z_mm_en_test.h"
}

static void MergeShip_RegisterMMEnTest() {
    if (gMergeShipActorId_En_Test >= 0) {
        return;
    }

    ActorDBInit entry;
    entry.name = "MM_En_Test";
    entry.desc = "Majora's Mask crater marks (Goron punch / ground pound)";
    entry.category = ACTORCAT_ITEMACTION;
    entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = sizeof(MMEnTest);
    entry.init = (ActorFunc)MMEnTest_Init;
    entry.destroy = (ActorFunc)MMEnTest_Destroy;
    entry.update = (ActorFunc)MMEnTest_Update;
    entry.draw = (ActorFunc)MMEnTest_Draw;

    gMergeShipActorId_En_Test = (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

static RegisterShipInitFunc sMergeShipMMEnTestInit(MergeShip_RegisterMMEnTest);
