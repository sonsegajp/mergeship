#pragma once

// MergeShip2 -- 2 Ship 2 Harkinian's projector for the host's shared save (include/mergeship/SharedSave.h in the
// MergeShip2 repository; M4 in Shipwright/mergeship2/docs/HOST_DESIGN.md): Majora's Mask's save <-> MsSharedSave.
//
//   capture  gSaveContext.save -> MsSharedSave   leaving through the Clock Tower's passage
//   apply    MsSharedSave -> gSaveContext.save   arriving from Hyrule
//
// Counters (life, magic, rupees, ammunition, Deku Seeds, bottle contents, the Powder Keg, trade items, stray fairies)
// are set both ways; masks, songs, items, capacities, equipment and Hyrule's quest status are added. Majora's Mask
// keeps only the equipped sword and shield, so a capture adds its Razor / Gilded Sword and Mirror Shield instead of
// setting the lists, and its starting Kokiri Sword and Hero's Shield (the gear Link brought) are never added to
// Hyrule's. Hyrule's own items, songs, quest status and equipment go to Termina's Hyrule views (Items (Hyrule),
// Hyrule's quest status, Equipment: MergeShipOoTItems.h, MergeShipHyrule.h, MergeShipOoTEquip.h) and come back.
//
// Majora's Mask's first cycle (MergeShipHost.cpp): with "Share items during Termina's first cycle" on
// (MS_CVAR_SHARE_FIRST_CYCLE) the shared save takes part from the first arrival, but an apply made while MM's first
// cycle is still its own script holds back what would short-circuit that script (MERGESHIP_SHARED_HOLD_FIRST_CYCLE):
// MM's Ocarina of Time (the Skull Kid has it until the Clock Tower's rooftop), the Song of Time (Zelda's memory on the
// rooftop), the Song of Healing and the transformation masks (a Deku, Goron, Zora or Fierce Deity Mask would let the
// cursed Deku take the curse off before the Happy Mask Salesman heals him; his scenes read the ocarina, the Deku Mask
// and the Song of Healing). MergeShipShared_ApplyHeldBack gives those once MM's own script is done.

#ifdef __cplusplus
extern "C" {
#endif

struct MsSharedSave;

// Nothing is granted (the user's rules, 2026-09-23): a Termina file the door creates starts with nothing of Majora's
// Mask's new-save gear (MergeShipHost.cpp CreateFile: no Kokiri Sword, Hero's Shield or magic meter) and marks itself
// with MERGESHIP_SHARED_BARE_START; Termina holds only what the shared save carries from Hyrule and what it earns. Each
// game's version of an item both games have stays that game's (SharedSave.h itemsModel): Hyrule's Hookshot / Longshot
// and Hylian / Mirror Shields go to Termina's Hyrule views, never to MM's Hookshot / Hero's / Mirror Shield, and MM's
// never become Hyrule's. Shared as one item (MsItems.def, and no second slot in Termina): the ocarina (MM's one
// ocarina; MM's counts as Hyrule's Fairy Ocarina), the bow and quiver, arrows, bombs, bombchus, sticks, nuts, beans,
// the Lens, bottles, the Kokiri Sword, wallet, hearts, the magic meter (only an owned one: the amount never makes one).
//
// MergeShipOoTEquipSave::variants bit 7 (MergeShipOoTEquip.h keeps bits 0-3): this Termina file was created by the door
// without MM's starting gear, so the Hero's Shield it holds was obtained in Termina (an older file's is MM's default).
#define MERGESHIP_SHARED_BARE_START 0x80

// MergeShipShared_Apply flags.
#define MERGESHIP_SHARED_HOLD_FIRST_CYCLE 1 // hold back MM's ocarina, Song of Time / Healing, transformation masks

void MergeShipShared_Capture(struct MsSharedSave* shared);
void MergeShipShared_Apply(const struct MsSharedSave* shared, int flags);
// Gives only what an apply with MERGESHIP_SHARED_HOLD_FIRST_CYCLE held back. Returns how many things it gave.
int MergeShipShared_ApplyHeldBack(const struct MsSharedSave* shared);

#ifdef __cplusplus
}
#endif
