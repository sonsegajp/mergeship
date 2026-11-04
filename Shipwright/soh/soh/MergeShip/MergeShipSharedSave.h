#pragma once

// MergeShip2 -- Ship of Harkinian's projector for the host's shared save (include/mergeship/SharedSave.h, M4 in
// Shipwright/mergeship2/docs/HOST_DESIGN.md): Ocarina of Time's save <-> MsSharedSave.
//
//   capture  gSaveContext / gMergeShipSave / gMergeShipMMItemsSave -> MsSharedSave (leaving through the door, and
//            when the file is saved)
//   apply    MsSharedSave -> the same (coming back through the door, and when the file is loaded)
//
// Both set, never add. Life, magic, rupees, ammunition, bottles and the Termina items that come and go (the Powder
// Keg, the trade items) are set both ways. Equipment and everything else that is only ever gained (masks, songs,
// the bow, the hookshot ...) is set from Hyrule on capture but only ever *added* on apply: Termina may give Hyrule
// something, it never takes Hyrule's own equipment away.
//
// Hyrule's own share (the shared save's Hyrule block and ootGear fields) goes to Termina's Hyrule views (2Ship's Items
// (Hyrule), Hyrule quest status and Equipment pages): songs, medallions, Spiritual Stones, the Stone of Agony, the
// Gerudo Card, the Slingshot/Boomerang/Hammer/Longshot/spells, the masks and trade items, Deku Seeds, swords, shields,
// tunics, boots, strength and scale. What Termina adds to them comes back here (added; the Deku Shield, which burns,
// and the Deku Seeds are set both ways; an empty trade slot takes a trade item Termina holds).

#ifdef __cplusplus
extern "C" {
#endif

struct MsSharedSave;

void MergeShipShared_Capture(struct MsSharedSave* shared);
void MergeShipShared_Apply(const struct MsSharedSave* shared);

#ifdef __cplusplus
}
#endif
