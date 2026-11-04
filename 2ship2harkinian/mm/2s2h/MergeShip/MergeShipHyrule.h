#pragma once

// MergeShip2 -- Hyrule in Termina: Ocarina of Time's songs, warp songs and quest status in Majora's Mask (2Ship).
// The reverse of SoH's MergeShipSongs / MergeShipTermina (MM's songs and owl warps in Ocarina of Time).
//
// Songs. MM's ocarina already knows Zelda's Lullaby (MM OCARINA_SONG_ZELDAS_LULLABY, 21: its notes and buttons are
// Ocarina of Time's), Saria's Song and the Sun's Song (MM OCARINA_SONG_SARIAS / _SUNS, with quest bits); it has no
// quest bit for the lullaby and no song at all for the six warp songs. Those get song indices past MM's table
// (MERGESHIP_SONG_MINUET..): OcarinaStaff::state, msgCtx->songPlayed / lastPlayedSong and
// AudioOcarina_SetPlaybackSong(song + 1) carry them like MM's own, and nothing of MM's numbering moves. Their notes
// are Ocarina of Time's (code_8019AF00.c already holds OoT's table as sOoTOcarinaSongNotes; it is byte-identical to
// Shipwright's code_800EC960.c sOcarinaSongNotes) and their buttons are OoT's gOcarinaSongButtons. They are matched in
// free play only (AudioOcarina_CheckSongsWithoutMusicStaff); no note sequence of one game is inside one of the other's.
//
// Learned songs, medallions, Spiritual Stones, the Stone of Agony and the Gerudo Card are Ocarina of Time's own
// QUEST_* bits, kept in the 2S2H save as ShipSaveInfo::mergeShipOoTQuest ("mergeShipOoTQuest"). Epona's Song, the
// Song of Time, the Song of Storms, Saria's Song and the Sun's Song are MM's own quest bits (QUEST_SONG_*).
//
// Played, a song does what Ocarina of Time does, on the current form's instrument as MM plays its songs: the notes
// are played back with sPlayerFormOcarinaInstruments[form] and Ocarina of Time's fanfare sequence (from oot/audio,
// oot-assets.o2r, registered in MM's sequence and font tables at AudioLoad_Init) accompanies them. "You played ..."
// is Ocarina of Time's text. A warp song then asks Ocarina of Time's "Warp to ...?" and, on OK, requests the engine
// transfer to Ocarina of Time's warp pad (MergeShip_RequestHyruleWarp).
//
// Everything is gated by MERGESHIP_OOT_SONGS_CVAR (default on).

#include "PR/ultratypes.h"
#include "z64ocarina.h"

#define MERGESHIP_OOT_SONGS_CVAR "gEnhancements.MergeShip.OoTSongs"

// Ocarina of Time's QuestItem numbering (OoT z64item.h), the bits of MergeShipOoTQuestSave::questItems.
typedef enum MergeShipOoTQuest {
    /*  0 */ MERGESHIP_OOT_QUEST_MEDALLION_FOREST,
    /*  1 */ MERGESHIP_OOT_QUEST_MEDALLION_FIRE,
    /*  2 */ MERGESHIP_OOT_QUEST_MEDALLION_WATER,
    /*  3 */ MERGESHIP_OOT_QUEST_MEDALLION_SPIRIT,
    /*  4 */ MERGESHIP_OOT_QUEST_MEDALLION_SHADOW,
    /*  5 */ MERGESHIP_OOT_QUEST_MEDALLION_LIGHT,
    /*  6 */ MERGESHIP_OOT_QUEST_SONG_MINUET,
    /*  7 */ MERGESHIP_OOT_QUEST_SONG_BOLERO,
    /*  8 */ MERGESHIP_OOT_QUEST_SONG_SERENADE,
    /*  9 */ MERGESHIP_OOT_QUEST_SONG_REQUIEM,
    /* 10 */ MERGESHIP_OOT_QUEST_SONG_NOCTURNE,
    /* 11 */ MERGESHIP_OOT_QUEST_SONG_PRELUDE,
    /* 12 */ MERGESHIP_OOT_QUEST_SONG_LULLABY,
    /* 13 */ MERGESHIP_OOT_QUEST_SONG_EPONA,  // MM's QUEST_SONG_EPONA in Termina
    /* 14 */ MERGESHIP_OOT_QUEST_SONG_SARIA,  // MM's QUEST_SONG_SARIA
    /* 15 */ MERGESHIP_OOT_QUEST_SONG_SUN,    // MM's QUEST_SONG_SUN
    /* 16 */ MERGESHIP_OOT_QUEST_SONG_TIME,   // MM's QUEST_SONG_TIME
    /* 17 */ MERGESHIP_OOT_QUEST_SONG_STORMS, // MM's QUEST_SONG_STORMS
    /* 18 */ MERGESHIP_OOT_QUEST_KOKIRI_EMERALD,
    /* 19 */ MERGESHIP_OOT_QUEST_GORON_RUBY,
    /* 20 */ MERGESHIP_OOT_QUEST_ZORA_SAPPHIRE,
    /* 21 */ MERGESHIP_OOT_QUEST_STONE_OF_AGONY,
    /* 22 */ MERGESHIP_OOT_QUEST_GERUDO_CARD,
    /* 23 */ MERGESHIP_OOT_QUEST_MAX
} MergeShipOoTQuest;

// Song indices on MM's ocarina for Ocarina of Time's warp songs, past MM's OCARINA_SONG_* (0..23, and 24 which
// AudioOcarina_SetPlaybackSong reads as the long Scarecrow's Song). Order: Ocarina of Time's OCARINA_SONG_MINUET..
// PRELUDE, so (song - MERGESHIP_SONG_MINUET) indexes Ocarina of Time's warp tables.
typedef enum MergeShipSong {
    /* 32 */ MERGESHIP_SONG_MINUET = 32,
    /* 33 */ MERGESHIP_SONG_BOLERO,
    /* 34 */ MERGESHIP_SONG_SERENADE,
    /* 35 */ MERGESHIP_SONG_REQUIEM,
    /* 36 */ MERGESHIP_SONG_NOCTURNE,
    /* 37 */ MERGESHIP_SONG_PRELUDE,
    /* 38 */ MERGESHIP_SONG_END
} MergeShipSong;

#define MERGESHIP_WARP_SONG_COUNT (MERGESHIP_SONG_END - MERGESHIP_SONG_MINUET)
#define MERGESHIP_SONG_IS_OOT_WARP(song) (((song) >= MERGESHIP_SONG_MINUET) && ((song) < MERGESHIP_SONG_END))

// MM ocarina mode for the answer to "Warp to ...?" (Ocarina of Time's warp prompt, OoT z_message_PAL.c:3801 with
// OCARINA_MODE_01). Past MM's OcarinaMode values, so no MM actor mistakes it for one of its own; while it is set the
// player keeps holding the ocarina (Player_Action_63 waits for OCARINA_MODE_END).
#define MERGESHIP_OCARINA_MODE_PROCESS_HYRULE_WARP 0x30

// Ocarina of Time's warp pads (OoT z_player.c sWarpSongEntrances), in Ocarina of Time's entrance numbering.
#define MERGESHIP_OOT_ENTR_SACRED_FOREST_MEADOW_WARP_PAD 0x600
#define MERGESHIP_OOT_ENTR_DEATH_MOUNTAIN_CRATER_WARP_PAD 0x4F6
#define MERGESHIP_OOT_ENTR_LAKE_HYLIA_WARP_PAD 0x604
#define MERGESHIP_OOT_ENTR_DESERT_COLOSSUS_WARP_PAD 0x1F1
#define MERGESHIP_OOT_ENTR_GRAVEYARD_WARP_PAD 0x568
#define MERGESHIP_OOT_ENTR_TEMPLE_OF_TIME_WARP_PAD 0x5F4

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// ---- State ---------------------------------------------------------------------------------------------------
s32 MergeShip_Hyrule_Enabled(void); // MERGESHIP_OOT_SONGS_CVAR
u32 MergeShip_Hyrule_GetQuestItems(void);
s32 MergeShip_Hyrule_HasQuestItem(u8 ootQuest);
void MergeShip_Hyrule_SetQuestItem(u8 ootQuest);
// Learned: Zelda's Lullaby and the warp songs from the MergeShip save; MM's songs (Saria's, Sun's...) from MM's bits.
s32 MergeShip_Hyrule_IsSongLearned(u8 song);
// Learned, enabled, and (for the Sun's Song) not excluded by the randomizer. Used by the ocarina and the staff.
s32 MergeShip_Hyrule_IsSongPlayable(u8 song);

// ---- Items (z_parameter.c) -------------------------------------------------------------------------------------
// Item_GiveImpl for ITEM_OOT_SONG_*, ITEM_OOT_STONE_OF_AGONY, ITEM_OOT_GERUDO_CARD. Returns true when handled.
s32 MergeShip_Hyrule_GiveItem(u8 item);
// A medallion / Spiritual Stone (no ItemId): MERGESHIP_OOT_QUEST_MEDALLION_* / _KOKIRI_EMERALD.. Returns false if
// ootQuest is not one.
s32 MergeShip_Hyrule_GiveQuestItem(u8 ootQuest);
// Owned (Item_CheckObtainability).
s32 MergeShip_Hyrule_IsHyruleItem(u8 item);
// Held above Link's head with Ocarina of Time's model and text (a GIEventGiveItem), then Item_Give / quest bit.
// `item` is an ITEM_OOT_* id, or 0x100 + MERGESHIP_OOT_QUEST_* for a medallion or stone.
void MergeShip_Hyrule_QueueGetItem(u16 item);
// Draws that get-item model (Ocarina of Time's, from oot/) at the current matrix, for `item` as above. Nothing for any
// other item or without the Ocarina of Time archive.
void MergeShip_Hyrule_DrawGetItemModel(struct PlayState* play, u16 item);

// ---- The ocarina (code_8019AF00.c) -----------------------------------------------------------------------------
// OoT's gOcarinaSongButtons for a warp song; MM's gOcarinaSongButtons for MM's indices; NULL otherwise.
OcarinaSongButtons* MergeShip_Ocarina_GetSongButtons(u8 song);

// ---- The song's steps (z_message.c) ----------------------------------------------------------------------------
// MSGMODE_SONG_PLAYED: Ocarina of Time's effect actor for the song (OoT Message_DrawMain sOcarinaEffectActorIds).
// Returns true when handled (the lullaby, Saria's Song).
s32 MergeShip_Hyrule_SpawnSongEffect(struct PlayState* play, u8 song);
// MSGMODE_SETUP_DISPLAY_SONG_PLAYED: Ocarina of Time's fanfare (OoT sOcarinaSongFanfares) with MM's io port 7 value
// for the form. Returns true when handled (the lullaby, the warp songs, Saria's Song).
s32 MergeShip_Hyrule_PlaySongFanfare(u8 song, u8 formIoData);
// MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN: Ocarina of Time's "You played ..." (OoT 0x893 + song). Returns true when
// handled (the lullaby, the warp songs, and Saria's and Sun's Song, whose MM texts 0x1B77 / 0x1B7D are empty).
s32 MergeShip_Hyrule_ContinueSongPlayedText(struct PlayState* play, u8 song);

// ---- The warp (MergeShipHyrule.cpp) ----------------------------------------------------------------------------
// The player said OK to "Warp to <place>?": hand Link to Ocarina of Time's engine at `ootEntrance` (Ocarina of Time's
// numbering, OoT sWarpSongEntrances). Hosted (mergeship.exe, MERGESHIP_HOSTED) this asks the host for the transfer
// (MsHostServices::RequestTransfer, include/mergeship/EngineApi.h); standalone there is no Hyrule, and MM's "Your
// notes echoed far... but nothing happened." (MM 0x1B95) answers. Returns true when the request reached a host.
s32 MergeShip_RequestHyruleWarp(u16 ootEntrance);
// Ocarina of Time's entrance for a warp song index.
u16 MergeShip_Hyrule_WarpSongEntrance(u8 song);

// ---- Arrival (MergeShipAdultLink.h) ----------------------------------------------------------------------------------
// The host transfer (Lost Woods -> Clock Tower) calls this before the player spawns, with Ocarina of Time's
// gSaveContext.linkAge (0 adult, 1 child): the human is Ocarina of Time's adult Link or MM's young Link, and the
// save keeps that age (ShipSaveInfo::mergeShipHumanAge).
void MergeShip_Hyrule_SetArrivalAge(u8 ootAge);

// ---- Audio (load.c) --------------------------------------------------------------------------------------------
// AudioLoad_Init: appends Ocarina of Time's song fanfares (oot/audio/sequences) to MM's sequence table and the sound
// fonts they play (oot/audio/fonts) to MM's font table.
void MergeShip_Audio_RegisterOoTSequences(void);
// MM's sequence id for Ocarina of Time's sequence `ootSeqId`, or 0xFFFF when it is not registered.
u16 MergeShip_Audio_OoTSeqId(u16 ootSeqId);

// ---- The quest page (z_kaleido_collect.c / z_kaleido_mergeship.c) -----------------------------------------------
// C-Up on MM's quest page switches it through its views: Termina's quest status (MM's own) -> Hyrule's quest status
// (Ocarina of Time's quest page: its medallions, Spiritual Stones, Stone of Agony, Gerudo Card and songs) -> Hyrule's
// equipment (Ocarina of Time's equipment page, MergeShipOoTEquipPause.c) -> back to Termina's, skipping a view whose
// option is off (MERGESHIP_OOT_SONGS_CVAR, MERGESHIP_OOT_EQUIP_CVAR). B on Hyrule's equipment goes back to Termina's
// quest status. A "[C-Up] Hyrule" / "Equipment" / "Termina" label under the page names the next view. The switch,
// the page's panels, its cursor and the label: z_kaleido_scope.h (MergeShip_QuestPage_*).
// The Hyrule quest status is the view shown.
s32 MergeShip_QuestHyrule_IsShown(struct PlayState* play);
void MergeShip_QuestHyrule_Draw(struct PlayState* play);
void MergeShip_QuestHyrule_UpdateCursor(struct PlayState* play);
void MergeShip_QuestHyrule_Reset(void);
// For the harness: show / hide the sub-page and place its cursor.
void MergeShip_QuestHyrule_Set(s32 shown, s16 point);
// Kaleido_LoadItemNameStatic: Ocarina of Time's name texture (oot/textures/item_name_static) for a sub-page cursor
// item (pauseCtx->namedItem), or NULL for any other item.
const char* MergeShip_QuestHyrule_NameTexture(u32 namedItem);
// Ocarina of Time's quest icon (oot/textures/icon_item_24_static) for a MERGESHIP_OOT_QUEST_* bit, or NULL (songs).
const char* MergeShip_Hyrule_QuestIconTexture(u8 ootQuest);

// Diagnostics (MergeShipTest.cpp); no-ops unless a harness script runs.
void MergeShipTest_Log(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
