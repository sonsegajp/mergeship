#pragma once

// MergeShip2 -- Majora's Mask's songs on Ocarina of Time's ocarina (MergeShipSongs.c, MergeShipSongs.cpp).
//
// Song indices. OcarinaStaff::state, msgCtx->lastPlayedSong and AudioOcarina_SetPlaybackSong(index + 1) all carry a
// song index, and AudioOcarina_Start takes one bit per song. Ocarina of Time's songs are 0..13 (OCARINA_SONG_*) and
// bits 14 and 15 of the flag word are its control bits (0x4000 free play / 0x8000 note-by-note), so Majora's Mask's
// songs start at 16: the flag word is widened to u32 (AudioOcarina_StartWide) and bit N is still song N. Nothing
// of Ocarina of Time's numbering moves, so every actor that compares lastPlayedSong with an OCARINA_SONG_* keeps
// working, and none of them can mistake an MM song for one of its own.
//
// MM's note sequences (MM code_8019AF00.c sOcarinaSongNotes / gOcarinaSongButtons) live in code_800EC960.c next to
// Ocarina of Time's. No MM sequence equals an Ocarina of Time one, and none is heard inside another, except the
// Goron Lullaby Intro inside the Goron Lullaby -- MM offers the intro only until the full lullaby is learned
// (MM Message_DisplayOcarinaStaffImpl), and so does this.
//
// Everything is gated by CVAR_ENHANCEMENT("MergeShip.MMSongs") (on by default).

#include "z64.h"
#include "soh/Enhancements/item-tables/ItemTableTypes.h"

#define MERGESHIP_SONG_FIRST 16
typedef enum MergeShipSong {
    /* 16 */ MERGESHIP_SONG_SONATA = MERGESHIP_SONG_FIRST, // MM OCARINA_SONG_SONATA
    /* 17 */ MERGESHIP_SONG_GORON_LULLABY,                 // MM OCARINA_SONG_GORON_LULLABY
    /* 18 */ MERGESHIP_SONG_NEW_WAVE,                      // MM OCARINA_SONG_NEW_WAVE
    /* 19 */ MERGESHIP_SONG_ELEGY,                         // MM OCARINA_SONG_ELEGY
    /* 20 */ MERGESHIP_SONG_OATH,                          // MM OCARINA_SONG_OATH
    /* 21 */ MERGESHIP_SONG_HEALING,                       // MM OCARINA_SONG_HEALING
    /* 22 */ MERGESHIP_SONG_SOARING,                       // MM OCARINA_SONG_SOARING
    /* 23 */ MERGESHIP_SONG_INVERTED_TIME,                 // MM OCARINA_SONG_INVERTED_TIME
    /* 24 */ MERGESHIP_SONG_DOUBLE_TIME,                   // MM OCARINA_SONG_DOUBLE_TIME
    /* 25 */ MERGESHIP_SONG_GORON_LULLABY_INTRO,           // MM OCARINA_SONG_GORON_LULLABY_INTRO
    /* 26 */ MERGESHIP_SONG_END
} MergeShipSong;

#define MERGESHIP_SONG_COUNT (MERGESHIP_SONG_END - MERGESHIP_SONG_FIRST)
#define MERGESHIP_SONG_IS_MM(song) (((song) >= MERGESHIP_SONG_FIRST) && ((song) < MERGESHIP_SONG_END))
#define MERGESHIP_SONG_FLAG(song) (1u << (song))
// Every MM song's bit in the widened flag word.
#define MERGESHIP_MM_SONG_FLAGS ((u32)(((1u << MERGESHIP_SONG_COUNT) - 1) << MERGESHIP_SONG_FIRST))

// gMergeShipSave.mmQuestItems (MM's own QUEST_* bits for what Hyrule has room for).
#define MERGESHIP_QUEST_REMAINS_ODOLWA (1 << 0)
#define MERGESHIP_QUEST_REMAINS_GOHT (1 << 1)
#define MERGESHIP_QUEST_REMAINS_GYORG (1 << 2)
#define MERGESHIP_QUEST_REMAINS_TWINMOLD (1 << 3)
#define MERGESHIP_QUEST_BOMBERS_NOTEBOOK (1 << 4)

// MM's text ids, served as custom messages (MergeShipSongs.cpp) under MergeShip's own ids. MergeShip2's songs own
// 0x93C0..0x93DF (MM items 0x93A0..0x93BF, stray fairies 0x93E0..).
#define MERGESHIP_TEXT_SONG_PLAYED_BASE 0x93C0 // + (song - MERGESHIP_SONG_FIRST): MM 0x1B72 + MM song ("You played")
#define MERGESHIP_TEXT_NOTHING_HAPPENED 0x93CA // MM 0x1B95 "Your notes echoed far...but nothing happened."
#define MERGESHIP_TEXT_NO_OWLS 0x93CB          // MM 0xFB "You have yet to leave your mark..."
#define MERGESHIP_TEXT_SOAR_TO 0x93CC          // MM 0x1B93 "Soar to <owl>?" (Yes / No)
#define MERGESHIP_TEXT_TIME_SLOW 0x93CD        // MM 0x1B8C "Slow the current flow of time?"
#define MERGESHIP_TEXT_TIME_NORMAL 0x93CE      // MM 0x1B8D "Return the flow of time to normal?"
#define MERGESHIP_TEXT_TIME_NOTHING 0x93CF     // MM 0x1B8B "But nothing happened..."
#define MERGESHIP_TEXT_TIME_NO_FURTHER 0x93D0  // MM 0x1B94 "You cannot proceed any further through time..."
#define MERGESHIP_TEXT_DOUBLE_TIME_BASE 0x93D1 // + 0..4: MM 0x1B8E..0x1B92 ("Proceed to the ...?")
#define MERGESHIP_TEXT_GET_BASE 0x93D6         // + (item - ITEM_MM_REMAINS_ODOLWA): MM GI texts 0x55..0x58, 0x50
#define MERGESHIP_TEXT_LAST 0x93DA

// The get-item entries' ids in the MOD_NONE table (registered at run time, above GI_MAX). MergeShip2's songs own
// 0x01A0..0x01AF (Chateau Romani 0x0180, stray fairies 0x0181.., MM items 0x0190..).
#define MERGESHIP_GI_MM_REMAINS_ODOLWA 0x01A0
#define MERGESHIP_GI_MM_REMAINS_GOHT 0x01A1
#define MERGESHIP_GI_MM_REMAINS_GYORG 0x01A2
#define MERGESHIP_GI_MM_REMAINS_TWINMOLD 0x01A3
#define MERGESHIP_GI_MM_BOMBERS_NOTEBOOK 0x01A4
// + (song - MERGESHIP_SONG_FIRST), 0x01A5..0x01AE: only carried to the item-receive hooks (no table entry, no model).
#define MERGESHIP_GI_MM_SONG_FIRST 0x01A5

// MM's own ocarina modes for the prompts that follow a song (MM OCARINA_MODE_PROCESS_INVERTED_TIME /
// PROCESS_DOUBLE_TIME), past Ocarina of Time's OCARINA_MODE_* so no actor reads them as one of its own; with
// OCARINA_MODE_01 Ocarina of Time's TEXT_DONE would take the Yes as a warp song's.
#define MERGESHIP_OCARINA_MODE_PROCESS_INVERTED_TIME 0x30
#define MERGESHIP_OCARINA_MODE_PROCESS_DOUBLE_TIME 0x31
// BUG-0144: Link holds still while the Elegy's shell appears (MM Player_Action_88; MergeShip_Songs_Update ends it).
#define MERGESHIP_OCARINA_MODE_ELEGY 0x32

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// ---- The ocarina (code_800EC960.c) ----------------------------------------------------------------------------
// AudioOcarina_Start with the u32 flag word (MM's AudioOcarina_Start takes a u32 too). Bits 0..15 are Ocarina of
// Time's, exactly as AudioOcarina_Start(u16) reads them; bits 16..25 are MM's songs. MM's songs are only matched
// in free play (0x4000 set: AudioOcarina_CheckSongsWithoutMusicStaff); the note-by-note and 8-note checks (song
// prompts, teaching) keep Ocarina of Time's songs only, so their error and success rules are untouched.
void AudioOcarina_StartWide(u32 ocarinaFlags);
// gOcarinaSongButtons for any song index (Ocarina of Time's table, or MM's for MM songs); NULL for other indices.
OcarinaSongButtons* MergeShip_Ocarina_GetSongButtons(u8 song);

// ---- The songs (MergeShipSongs.c) -----------------------------------------------------------------------------
// CVAR_ENHANCEMENT("MergeShip.MMSongs").
s32 MergeShip_Songs_Enabled(void);
// Learned (MM's songs in the MergeShip save; the Inverted Song of Time and the Song of Double Time come with the
// Song of Time, as in MM).
s32 MergeShip_Songs_IsLearned(u8 song);
// The MM bits to add to Message_StartOcarina's flag word (MM Message_DisplayOcarinaStaffImpl): 0 when disabled.
u32 MergeShip_Songs_GetAvailableFlags(void);
// The song taught by an MM song item (ITEM_MM_SONG_*), or 0xFF.
u8 MergeShip_Songs_ItemToSong(u8 item);
// Item_Give for ITEM_MM_SONG_*, ITEM_MM_REMAINS_* and ITEM_MM_BOMBERS_NOTEBOOK (MM Item_Give's quest-item branch).
// Returns true when `item` was one of them.
s32 MergeShip_Songs_GiveItem(u8 item);
// Owned: an MM song item, remains or the notebook.
s32 MergeShip_Songs_HasItem(u8 item);
// MM's get-item entry for a remains / the notebook (MM's model and text), or a song (no model; for the item-receive
// hooks); GET_ITEM_NONE for other items.
GetItemEntry MergeShip_Songs_GetItemEntry(u8 item);

// z_message_PAL.c, the song's steps (MM z_message.c).
// MSGMODE_SONG_PLAYED: MM Message_SpawnSongEffect.
void MergeShip_Songs_SpawnEffect(struct PlayState* play, u8 song);
// MSGMODE_SETUP_DISPLAY_SONG_PLAYED: MM Audio_PlayFanfareWithPlayerIOPort7(sOcarinaSongFanfares[song],
// sOcarinaSongFanfareIoData[form]) -- the song's MM fanfare, on the current form's instrument.
void MergeShip_Songs_PlayFanfare(u8 song);
// MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN: "You played the ..." (MM 0x1B72 + song).
u16 MergeShip_Songs_PlayedTextId(u8 song);
// MSGMODE_SONG_PLAYED_ACT, for an MM song (MM MSGMODE_18): closes the box and sets the ocarina mode, or opens MM's
// "nothing happened" box for the Elegy outside Ikana.
void MergeShip_Songs_OnSongPlayedAct(struct PlayState* play, u8 song);
// Message_Update MSGMODE_TEXT_CLOSING (MM's Song of Soaring / Inverted / Double Time branches). Returns true when it
// took over the closing (a new box or the owl-warp map was opened).
s32 MergeShip_Songs_OnTextClosing(struct PlayState* play);
// Message_Update MSGMODE_TEXT_DONE with a two-choice box (MM's OCARINA_MODE_PROCESS_INVERTED_TIME /
// PROCESS_DOUBLE_TIME answers). Returns true when it handled the input.
s32 MergeShip_Songs_OnChoice(struct PlayState* play);
// z_kankyo.c: the day's time step, slowed by the Inverted Song of Time (MM R_TIME_SPEED + timeSpeedOffset: 3 -> 1).
u16 MergeShip_Songs_TimeStep(u16 timeSpeed);
// Once per frame (MergeShipSongs.cpp): the Song of Double Time's fast-forward.
void MergeShip_Songs_Update(struct PlayState* play);

// ---- The quest page (z_kaleido_mergeship.c) ---------------------------------------------------------------------
// The Termina quest status sub-page: MM's quest page layout (z_kaleido_collect.c) for what Hyrule's quest page has no
// room for -- the four remains, MM's songs (with the Song of Time, Epona's Song and the Song of Storms in MM's
// places), the Bomber's Notebook, and MM's sword, shield, quiver and bomb bag boxes. C-Down on Ocarina of Time's quest
// page switches between the two (CVAR_ENHANCEMENT("MergeShip.MMSongs.QuestPage"), on by default;
// MergeShip_Kaleido_UpdateWorldSwitch, which switches the Item and Equipment faces the same way).
// KaleidoScope_DrawPageFace asks MergeShip_QuestTermina_IsShown and then draws the sub-page instead of Ocarina of
// Time's quest status.
s32 MergeShip_QuestTermina_IsShown(struct PlayState* play);
// C-Up on the Quest face belongs to the world switch (the randomizer's misc collectibles page is not opened).
s32 MergeShip_QuestTermina_OwnsCUp(struct PlayState* play);
void MergeShip_QuestTermina_Draw(struct PlayState* play);
// The pause menu closed: the next pause opens on Ocarina of Time's quest page again.
void MergeShip_QuestTermina_Reset(void);
// The pause menu closed: every page's C-Down view (Select Item, Quest Status, Equipment) back to Ocarina of Time's.
void MergeShip_Kaleido_ResetWorldViews(struct PlayState* play);

#ifdef __cplusplus
}
#endif
