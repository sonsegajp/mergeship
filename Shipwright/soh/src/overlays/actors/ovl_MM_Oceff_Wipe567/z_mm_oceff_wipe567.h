#ifndef Z_MM_OCEFF_WIPE567_H
#define Z_MM_OCEFF_WIPE567_H

// MergeShip2 -- Majora's Mask's three song effects that Ocarina of Time lacks (MM ovl_Oceff_Wipe5, ovl_Oceff_Wipe6,
// ovl_Oceff_Wipe7), for MM's songs played in Hyrule (MergeShipSongs.c, MM Message_SpawnSongEffect):
//   Wipe5 -- Sonata of Awakening, Goron Lullaby, New Wave Bossa Nova, Elegy of Emptiness, Oath to Order
//            (params 0..4 pick the colours, MM sOcarinaEffectActorParams)
//   Wipe6 -- Song of Soaring
//   Wipe7 -- Song of Healing
// Registered at boot with ActorDB (soh/MergeShip/MergeShipActorMMOceff.cpp); their ids are published below.

#include "global.h"

#define MM_OCEFF_WIPE5_GET_SONG_TYPE(thisx) ((thisx)->params & 0xF)

typedef struct MMOceffWipe5 {
    /* 0x000 */ Actor actor;
    /* 0x144 */ s16 counter;
} MMOceffWipe5; // size = 0x148

typedef struct MMOceffWipe6 {
    /* 0x000 */ Actor actor;
    /* 0x144 */ s16 counter;
} MMOceffWipe6; // size = 0x148

typedef struct MMOceffWipe7 {
    /* 0x000 */ Actor actor;
    /* 0x144 */ s16 counter;
} MMOceffWipe7; // size = 0x148

void MMOceffWipe5_Init(Actor* thisx, PlayState* play);
void MMOceffWipe5_Destroy(Actor* thisx, PlayState* play);
void MMOceffWipe5_Update(Actor* thisx, PlayState* play);
void MMOceffWipe5_Draw(Actor* thisx, PlayState* play);

void MMOceffWipe6_Init(Actor* thisx, PlayState* play);
void MMOceffWipe6_Destroy(Actor* thisx, PlayState* play);
void MMOceffWipe6_Update(Actor* thisx, PlayState* play);
void MMOceffWipe6_Draw(Actor* thisx, PlayState* play);

void MMOceffWipe7_Init(Actor* thisx, PlayState* play);
void MMOceffWipe7_Destroy(Actor* thisx, PlayState* play);
void MMOceffWipe7_Update(Actor* thisx, PlayState* play);
void MMOceffWipe7_Draw(Actor* thisx, PlayState* play);

// ActorDB ids (-1 until registered, or without mm.o2r).
extern s16 gMergeShipActorId_Oceff_Wipe5;
extern s16 gMergeShipActorId_Oceff_Wipe6;
extern s16 gMergeShipActorId_Oceff_Wipe7;

// MM msgCtx->ocarinaSongEffectActive: set when a song effect is spawned (MergeShipSongs.c), cleared by the
// effect's Destroy. MM's Song of Soaring waits for it before opening the owl-warp map.
extern s32 gMergeShipSongEffectActive;

#endif
