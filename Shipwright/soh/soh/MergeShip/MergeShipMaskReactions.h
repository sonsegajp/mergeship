#pragma once

// MergeShip2 -- Majora's Mask masks that change how Ocarina of Time's actors treat the wearer.
//
// MM has no generic "mask reaction" system: each actor checks `Player_GetMask(play) == PLAYER_MASK_*` in its own
// code. The OoT counterparts make the same checks, in the same places, through the gates below. Every gate is an
// optional enhancement (CVAR_ENHANCEMENT("MergeShip.<Name>"), default 1):
//
//   Captain's Hat        MM ovl_En_Skb        Stalchildren turn friendly, stop and talk   MergeShip.CaptainsHat
//                        (randomizer)         ...and report a gossip-stone hint          MergeShip.CaptainsHatHints
//   Gibdo Mask           MM ovl_En_Railgibud  Gibdos stop hunting, walk home, can talk    MergeShip.GibdoMask
//   Gibdo/Captain's/Garo MM ovl_En_Rd         ReDeads dance instead of attacking          MergeShip.RedeadDance
//   Mask of Truth        OoT's own checks     reads Gossip Stones like OoT's mask         MergeShip.TruthGossipStones
//                        (randomizer)         a stone gives one extra hint                MergeShip.TruthExtraHint
//
// The dialogue is MM's English text (mm.o2r text/message_data_static), shown through SoH's custom message system
// under the text ids below (MergeShipMaskMessages.cpp builds them).

#include <libultraship/libultra/types.h>

// MM 0x13F6 "Huh?" [event] -- a Stalchild's first reaction to the Captain's Hat.
#define MERGESHIP_TEXT_SKB_HUH 0x9380
// MM 0x13F7 "Oh, Captain! / E-excuse me, sir!" -- follows 0x13F6. Randomizer: the Stalchild's hint follows it.
#define MERGESHIP_TEXT_SKB_CAPTAIN 0x9381
// MM 0x13F8 "The squad leader is in front of / that grave over there, sir." -- every later talk. Randomizer: the
// Stalchild reports its hint instead.
#define MERGESHIP_TEXT_SKB_SQUAD_LEADER 0x9382
// MM 0x13B2 "Brrrinnnggg them!" [event] -- a Gibdo spoken to through the Gibdo Mask (MM EnRailgibud).
#define MERGESHIP_TEXT_GIBDO_BRING_THEM 0x9383
// MM 0x13B3 "Those inssside...Our friendsss... / Brrrriiiiinnnng them!" -- follows 0x13B2.
#define MERGESHIP_TEXT_GIBDO_OUR_FRIENDS 0x9384

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// MM `Player_GetMask(play) == PLAYER_MASK_CAPTAIN` in z_en_skb.c, with MergeShip.CaptainsHat on.
s32 MergeShip_CaptainsHatWorn(struct PlayState* play);
// MM `Player_GetMask(play) == PLAYER_MASK_GIBDO` in z_en_railgibud.c / z_en_talk_gibud.c, with MergeShip.GibdoMask on.
s32 MergeShip_GibdoMaskWorn(struct PlayState* play);
// !MM EnRd_ShouldNotDance (z_en_rd.c:327): the Gibdo Mask, Captain's Hat or Garo's Mask, with MergeShip.RedeadDance on.
s32 MergeShip_RedeadsShouldDance(struct PlayState* play);
// OoT's `Player_GetMask(play) == PLAYER_MASK_TRUTH` for reading Gossip Stones: OoT's Mask of Truth, or MM's with
// MergeShip.TruthGossipStones on. Not used for the Deku Scrub mask contest, which judges OoT's masks only.
s32 MergeShip_ReadsGossipStones(struct PlayState* play);
// BUG-0127: Kakariko's gate guard (z_en_heishi2.c) takes MM's Keaton Mask (MM) as a Keaton Mask: he asks for it and
// pays as for Ocarina of Time's, once (gMergeShipSave.guardSawMMKeaton), and Link keeps it (the user: "Make sure he
// doesnt actually lose keaton"). It is not the Happy Mask Shop's mask, so the shop's sequence (the guard's flags, SOLD
// OUT) is left as it was: a Keaton Mask borrowed from the shop still sells to him.
s32 MergeShip_GuardWantsMMKeaton(struct PlayState* play);
void MergeShip_GuardTakesMMKeaton(void);

#ifdef __cplusplus
}
#endif
