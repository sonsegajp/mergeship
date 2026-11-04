// MergeShip2 -- registers Majora's Mask's En_Clear_Tag (explosions, light rays, splashes, smoke;
// src/overlays/actors/ovl_MM_En_Clear_Tag) with ActorDB at boot. Ocarina of Time's En_Clear_Tag (the Arwing) keeps
// ACTOR_EN_CLEAR_TAG; MM's takes the next free id, published as gMergeShipActorId_MM_En_Clear_Tag (the powder keg's
// explosion, ovl_MM_En_Bom_Keg).
//
// This file lives under soh/soh/MergeShip rather than beside the actor because the soh target only globs
// .c/.h files under src/.

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MM_En_Clear_Tag/z_mm_en_clear_tag.h"
}

static void MergeShip_RegisterMMEnClearTag() {
    if (gMergeShipActorId_MM_En_Clear_Tag >= 0) {
        return;
    }

    ActorDBInit entry;
    entry.name = "MM_En_Clear_Tag";
    entry.desc = "Majora's Mask explosion / light ray / splash effects";
    entry.category = ACTORCAT_ITEMACTION;
    // MM: ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | UPDATE / DRAW culling disabled (Init clears attention).
    entry.flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                  ACTOR_FLAG_DRAW_CULLING_DISABLED;
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = sizeof(MMEnClearTag);
    entry.init = (ActorFunc)MMEnClearTag_Init;
    entry.destroy = (ActorFunc)MMEnClearTag_Destroy;
    entry.update = (ActorFunc)MMEnClearTag_Update;
    entry.draw = (ActorFunc)MMEnClearTag_Draw;

    gMergeShipActorId_MM_En_Clear_Tag = (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

static RegisterShipInitFunc sMergeShipMMEnClearTagInit(MergeShip_RegisterMMEnClearTag);
