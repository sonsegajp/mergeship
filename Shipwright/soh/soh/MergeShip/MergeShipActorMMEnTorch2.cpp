// MergeShip2 (BUG-0144) -- registers Majora's Mask's Elegy of Emptiness shell (MM En_Torch2) and its beam of light (MM
// Eff_Change), src/overlays/actors/ovl_MM_En_Torch2, with ActorDB at boot. Ocarina of Time has neither; each takes the
// next free id, published as gMergeShipActorId_En_Torch2 / _Eff_Change for MergeShipSongs.c (the Elegy in Hyrule).
//
// This file lives under soh/soh/MergeShip rather than beside the actors because the soh target only globs .c/.h files
// under src/.

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_MM_En_Torch2/z_mm_en_torch2.h"
}

static s16 RegisterMMActor(const char* name, const char* desc, ActorFunc init, ActorFunc destroy, ActorFunc update,
                           ActorFunc draw, size_t size) {
    ActorDBInit entry;
    entry.name = name;
    entry.desc = desc;
    entry.category = ACTORCAT_ITEMACTION;
    entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED; // MM FLAGS of both
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = (int)size;
    entry.init = init;
    entry.destroy = destroy;
    entry.update = update;
    entry.draw = draw;
    return (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

static void MergeShip_RegisterMMEnTorch2() {
    if (gMergeShipActorId_En_Torch2 >= 0) {
        return;
    }
    gMergeShipActorId_En_Torch2 =
        RegisterMMActor("MM_En_Torch2", "Majora's Mask Elegy of Emptiness shell", (ActorFunc)MMEnTorch2_Init,
                        (ActorFunc)MMEnTorch2_Destroy, (ActorFunc)MMEnTorch2_Update, (ActorFunc)MMEnTorch2_Draw,
                        sizeof(MMEnTorch2));
    gMergeShipActorId_Eff_Change =
        RegisterMMActor("MM_Eff_Change", "Majora's Mask Elegy of Emptiness beam of light", (ActorFunc)MMEffChange_Init,
                        (ActorFunc)MMEffChange_Destroy, (ActorFunc)MMEffChange_Update, (ActorFunc)MMEffChange_Draw,
                        sizeof(MMEffChange));
}

static RegisterShipInitFunc sMergeShipMMEnTorch2Init(MergeShip_RegisterMMEnTorch2);
