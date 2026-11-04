#pragma once
// MergeShip2 -- singleplayer mode (docs/SINGLEPLAYER.md): one Mergeship menu option, never on a randomizer file.
//
// Phase 1, the traveling Happy Mask Salesman (Hyrule only):
//   * a timer re-rolls where he is -- his shop in Castle Town Market, or roaming Castle Town, Kakariko, Death
//     Mountain Trail, Zora's River, Zora's Domain or Hyrule Field; the roll is per file and per time slot (a quarter
//     of a day), so a reload does not move him. While he is out his Market shop is closed.
//   * he walks on the ground (MM's own model and walk, ovl_MM_En_Osn_Travel) and sells Majora's Mask's masks, three
//     at a time from a rotation that shows every unowned mask before any comes round again;
//   * the Adult Mask only when he meets Link as an adult; the Fierce Deity's Mask as his gift once every mask is
//     bought.

#include <libultraship/libultra.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Actor;

#define MERGESHIP_SINGLEPLAYER_CVAR "gEnhancements.MergeShip.SPMode"
// 0: the Fierce Deity's Mask only from the salesman; 1: from the salesman or, as in MM, the moon.
#define MERGESHIP_SINGLEPLAYER_FD_MOON_CVAR "gEnhancements.MergeShip.SPFierceDeityFromMoon"

// The salesman's texts (MergeShipSingleplayer.cpp builds them on OnOpenText).
#define MERGESHIP_TEXT_SP_GREET 0x9400       // hello (first meeting of a visit)
#define MERGESHIP_TEXT_SP_MENU 0x9401        // the three masks of the rotation (a choice)
#define MERGESHIP_TEXT_SP_CONFIRM 0x9402     // "<mask> for <price> Rupees?" Buy / No thanks
#define MERGESHIP_TEXT_SP_BOUGHT 0x9403      // got the mask
#define MERGESHIP_TEXT_SP_NO_RUPEES 0x9404   // not enough rupees
#define MERGESHIP_TEXT_SP_FAREWELL 0x9405    // goodbye
#define MERGESHIP_TEXT_SP_SOLD_OUT 0x9406    // nothing left to sell
#define MERGESHIP_TEXT_SP_ADULT 0x9407       // "you've grown up" -- and the Adult Mask (Buy / No thanks)
#define MERGESHIP_TEXT_SP_FD_GIFT 0x9408     // every mask bought: the Fierce Deity's Mask
#define MERGESHIP_TEXT_SP_DOOR_ASK 0x9409   // at the Lost Woods: bring the masks to that other world? (a choice)
#define MERGESHIP_TEXT_SP_DOOR_BRING 0x940A // yes: take them along -- then he walks away
#define MERGESHIP_TEXT_SP_DOOR_SOLD 0x940B  // no: he buys them all back -- then he walks away
#define MERGESHIP_TEXT_SP_DOOR_NONE 0x940C  // no masks to bring: he walks away
#define MERGESHIP_TEXT_SP_NAVI_END 0x7E40   // Navi after the ending (below 0x8000: z_player.c forces her talk with -id, an s16)
#define MERGESHIP_TEXT_SP_NAVI_HINT 0x940E  // Navi's C-Up hint in the post-game, until the Lost Woods
#define MERGESHIP_TEXT_SP_SHOP_CLOSED 0x023F // the Market shop's door sign while he is out (EnDoor 0x0200 + 0x3F)

// The places he can be (MergeShip_Singleplayer_SalesmanPlace).
typedef enum MsSalesmanPlace {
    MS_SALESMAN_SHOP,
    MS_SALESMAN_MARKET,
    MS_SALESMAN_KAKARIKO,
    MS_SALESMAN_DEATH_MOUNTAIN,
    MS_SALESMAN_ZORAS_RIVER,
    MS_SALESMAN_ZORAS_DOMAIN,
    MS_SALESMAN_HYRULE_FIELD,
    MS_SALESMAN_PLACE_MAX,
} MsSalesmanPlace;

// ---- the schedule (MergeShipSingleplayer.cpp) ----
// What a salesman actor does when it starts: roam the hub (stay), or walk waypoints (travel) -- then stay at the hub
// (endAtHub), or leave the scene (fading if fadeOut).
#define MS_SALESMAN_PLAN_STAY 0
#define MS_SALESMAN_PLAN_TRAVEL 1
#define MS_SALESMAN_PLAN_DOOR 2 // the post-game Lost Woods: he waits ahead of Link, speaks, and walks away
#define MS_SALESMAN_PLAN_MAX_WAY 24
typedef struct MsSalesmanPlan {
    s32 mode;
    f32 start[3];
    f32 way[MS_SALESMAN_PLAN_MAX_WAY][3];
    s32 wayCount;
    f32 hub[3];
    f32 radius;
    s32 stay; // the stay he belongs to (StayOver says when it ends)
    u8 fadeIn;
    u8 fadeOut;
    u8 endAtHub;
} MsSalesmanPlan;
// The plan the next salesman actor takes in its Init (set by the spawner just before Actor_Spawn).
extern MsSalesmanPlan gMsSalesmanPlan;
// The stay is over: time for him to leave.
s32 MergeShip_Singleplayer_StayOver(s32 stay);
// His way out of the scene he stayed in (true when there is one here).
s32 MergeShip_Singleplayer_DeparturePlan(MsSalesmanPlan* plan);
// He walked out of this scene (or faded away): not spawned again here for this stay.
void MergeShip_Singleplayer_LeftScene(s32 stay);
// His clock (frames of play), and setting it (tests).
u32 MergeShip_Singleplayer_Clock(void);
void MergeShip_Singleplayer_SetClock(u32 clock);
// Logs where the timeline has him (the game log, and the harness trace).
void MergeShip_Singleplayer_LogWhere(void);
u32 MergeShip_Singleplayer_FindTravel(s16 sceneNum, f32 minOffset, s32 arriving);

// The option is on and the loaded file is not a randomizer file.
s32 MergeShip_Singleplayer_Active(void);
// Where he stays now (-1 while he walks between places).
s32 MergeShip_Singleplayer_SalesmanPlace(void);

// ---- the shop (the actor drives the conversation; these hold the stock) ----
// Called when a conversation starts: picks the rotation's three masks for this visit.
void MergeShip_Singleplayer_ShopBegin(void);
// How many masks are offered now (0..3), and the item id of offer i.
s32 MergeShip_Singleplayer_OfferCount(void);
// The menu's choice: makes offer `choice` the one the confirm text names. False when out of range.
s32 MergeShip_Singleplayer_ChooseOffer(s32 choice);
// Buy the chosen offer: true (rupees taken, mask given) or false (not enough rupees).
s32 MergeShip_Singleplayer_BuyChosen(void);
// The Adult Mask: offered (adult Link, not bought yet) / bought.
s32 MergeShip_Singleplayer_AdultMaskOffered(void);
s32 MergeShip_Singleplayer_BuyAdultMask(void);
s32 MergeShip_Singleplayer_AdultMaskBought(void);
// Every mask he sells is owned and the Fierce Deity's Mask not yet given: give it (true).
s32 MergeShip_Singleplayer_GiveFierceDeityIfDone(void);
// Nothing left for him to sell (the gift given too).
s32 MergeShip_Singleplayer_SoldOut(void);

// ---- the end of Ocarina of Time and the post-game (phases 2 and 3) ----
// Ganon is beaten in singleplayer (the farewell's end took Link back to the Temple of Time).
s32 MergeShip_Singleplayer_OotBeaten(void);
// Singleplayer, and Ocarina of Time not beaten yet: Termina (the Lost Woods door, the owl warps) and the pause
// menu's C-Up Termina pages stay shut. Never true on a randomizer file.
s32 MergeShip_Singleplayer_TerminaLocked(void);
// z_demo.c terminator 117 (the end of Zelda's farewell, before the credits): in singleplayer, child Link goes to the
// Temple of Time instead, Navi stays. True when it took over the transition.
s32 MergeShip_Singleplayer_OnEnding(struct PlayState* play);
// The Lost Woods meeting is still to come (post-game, singleplayer).
s32 MergeShip_Singleplayer_DoorPending(void);
// No: every mask he sold goes back to him; returns the rupees paid (0 when there was nothing to sell).
s32 MergeShip_Singleplayer_SellAllMasks(void);
// Any mask he sold is owned (otherwise the meeting has nothing to ask).
s32 MergeShip_Singleplayer_HasAnyMask(void);
// He walked away: the Lost Woods lead on to Termina.
void MergeShip_Singleplayer_DoorDone(void);
// Tests: set the post-game flags.
void MergeShip_Singleplayer_DebugSetState(s32 ootBeaten, s32 doorDone);

#ifdef __cplusplus
}
#endif
