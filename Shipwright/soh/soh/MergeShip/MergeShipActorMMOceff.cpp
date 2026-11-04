// MergeShip2 -- registers Majora's Mask's Oceff_Wipe5/6/7 (src/overlays/actors/ovl_MM_Oceff_Wipe567) with ActorDB at
// boot: the song effects of MM's Sonata, Goron Lullaby, New Wave Bossa Nova, Elegy, Oath (Wipe5), Song of Soaring
// (Wipe6) and Song of Healing (Wipe7), which Ocarina of Time has no actor for. Each takes the next free id,
// published as gMergeShipActorId_Oceff_Wipe5/6/7 for MergeShipSongs.c (MM Message_SpawnSongEffect).
//
// This file lives under soh/soh/MergeShip rather than beside the actors because the soh target only globs .c/.h files
// under src/.

#include <cstring>

#include <ship/resource/Resource.h>

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"
#include "soh/ResourceManagerHelpers.h"

extern "C" {
#include "src/overlays/actors/ovl_MM_Oceff_Wipe567/z_mm_oceff_wipe567.h"
}

// 2S2H ResourceMgr_GetVtxArraySizeByName: the vertex count of an MM Vtx array resource (MM's song effects fade every
// second vertex of their frustum).
extern "C" size_t MergeShip_GetVtxCount(const char* path) {
    if (path == nullptr) {
        return 0;
    }
    if (strncmp(path, "__OTR__", 7) == 0) {
        path += 7;
    }
    // An Array resource of vertices: ArrayCount F3DVtx, each the size of a Vtx.
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(path);
    return (res != nullptr) ? (res->GetPointerSize() / sizeof(Vtx)) : 0;
}

static s16 RegisterOceff(const char* name, const char* desc, ActorFunc init, ActorFunc destroy, ActorFunc update,
                         ActorFunc draw, size_t size) {
    ActorDBInit entry;
    entry.name = name;
    entry.desc = desc;
    entry.category = ACTORCAT_ITEMACTION;
    // MM FLAGS: ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA
    entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA;
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = (int)size;
    entry.init = init;
    entry.destroy = destroy;
    entry.update = update;
    entry.draw = draw;
    return (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

static void MergeShip_RegisterMMOceff() {
    if (gMergeShipActorId_Oceff_Wipe5 >= 0) {
        return;
    }
    gMergeShipActorId_Oceff_Wipe5 =
        RegisterOceff("MM_Oceff_Wipe5", "Majora's Mask Sonata/Lullaby/Bossa Nova/Elegy/Oath effect",
                      (ActorFunc)MMOceffWipe5_Init, (ActorFunc)MMOceffWipe5_Destroy, (ActorFunc)MMOceffWipe5_Update,
                      (ActorFunc)MMOceffWipe5_Draw, sizeof(MMOceffWipe5));
    gMergeShipActorId_Oceff_Wipe6 =
        RegisterOceff("MM_Oceff_Wipe6", "Majora's Mask Song of Soaring effect", (ActorFunc)MMOceffWipe6_Init,
                      (ActorFunc)MMOceffWipe6_Destroy, (ActorFunc)MMOceffWipe6_Update, (ActorFunc)MMOceffWipe6_Draw,
                      sizeof(MMOceffWipe6));
    gMergeShipActorId_Oceff_Wipe7 =
        RegisterOceff("MM_Oceff_Wipe7", "Majora's Mask Song of Healing effect", (ActorFunc)MMOceffWipe7_Init,
                      (ActorFunc)MMOceffWipe7_Destroy, (ActorFunc)MMOceffWipe7_Update, (ActorFunc)MMOceffWipe7_Draw,
                      sizeof(MMOceffWipe7));
}

static RegisterShipInitFunc sMergeShipMMOceffInit(MergeShip_RegisterMMOceff);
