#pragma once

// MergeShip2 -- young Link rides Majora's Mask's young Epona in Hyrule.
//
// With CVAR_ENHANCEMENT("MergeShip.YoungEpona") on (the default), mm.o2r mounted, young Link
// can call Epona once he knows Epona's Song. There is one Epona (one gSaveContext.horseData): Ocarina of Time's
// En_Horse is drawn and behaves as MM's HORSE_TYPE_2 (object_horse_link_child: its model, size, rider point, mount
// animations and kid-horse sounds) while young Link is about, and as Ocarina of Time's adult Epona otherwise.
// Lon Lon Ranch keeps its foal (En_Horse_Link_Child) for young Link; En_Horse is not placed there for him.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// The option and its requirement (mm.o2r).
s32 MergeShip_YoungEpona_Enabled(void);
// Young Link is about: the young Epona is the one in play.
s32 MergeShip_Epona_IsYoung(void);
// Epona comes to this Link: the adult once Epona is won (EVENTCHKINF_EPONA_OBTAINED), young Link once he knows
// Epona's Song (with the option on).
s32 MergeShip_Epona_Owned(void);
// Epona's Song was just played (z_message_PAL.c, DREG(53)): young Link's Epona answers even when none was placed when
// the scene loaded (the song found in this very scene: Horse_InitPlayerHorse only runs at a scene's start).
void MergeShip_Epona_OnSongPlayed(PlayState* play);

// BUG-0136: an Anchor player's horse (z_en_horse.c), drawn under their dummy while they ride (DummyPlayer.cpp): spawned
// at the rider, then placed under them each frame -- Epona, young Epona or Ganondorf's horse as they ride it, facing
// their way, standing, walking, trotting or galloping with their speed. It goes away when not placed for a few frames.
Actor* EnHorse_MergeShipSpawnPuppet(PlayState* play, const Vec3f* riderPos, s16 yaw);
void EnHorse_MergeShipPlacePuppet(Actor* horse, PlayState* play, const Vec3f* riderPos, s16 yaw, s32 young, s32 ganon);

#ifdef __cplusplus
}
#endif
