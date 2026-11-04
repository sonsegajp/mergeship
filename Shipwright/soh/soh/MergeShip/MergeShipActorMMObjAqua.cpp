// MergeShip2 -- registers Majora's Mask's Obj_Aqua (src/overlays/actors/ovl_MM_Obj_Aqua, the water poured out
// of a bottle) with ActorDB at boot. Ocarina of Time has no such actor; this one takes the next free id,
// published as gMergeShipActorId_MM_Obj_Aqua for the player to spawn where MM's Player_Action_70 does.
//
// This file lives under soh/soh/MergeShip rather than beside the actor because the soh target only globs
// .c/.h files under src/.

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MM_Obj_Aqua/z_mm_obj_aqua.h"
}

static void MergeShip_RegisterMMObjAqua() {
    if (gMergeShipActorId_MM_Obj_Aqua >= 0) {
        return;
    }

    ActorDBInit entry;
    entry.name = "MM_Obj_Aqua";
    entry.desc = "Majora's Mask water poured out of a bottle";
    entry.category = ACTORCAT_ITEMACTION;
    entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = sizeof(MMObjAqua);
    entry.init = (ActorFunc)MMObjAqua_Init;
    entry.destroy = (ActorFunc)MMObjAqua_Destroy;
    entry.update = (ActorFunc)MMObjAqua_Update;
    entry.draw = (ActorFunc)MMObjAqua_Draw;

    gMergeShipActorId_MM_Obj_Aqua = (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

static RegisterShipInitFunc sMergeShipMMObjAquaInit(MergeShip_RegisterMMObjAqua);
