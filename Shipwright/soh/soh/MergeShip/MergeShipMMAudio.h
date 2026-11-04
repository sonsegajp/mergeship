#pragma once

// MergeShip2 -- Majora's Mask sound effects inside Ocarina of Time: the pieces Ocarina of Time's audio
// engine calls into. The design is described at the top of MergeShipAudio.cpp.
//
// Three files implement it:
//   MergeShipAudio.cpp       loads MM's sequence 0 and its sound fonts out of mm.o2r (resource side).
//   MergeShipMMSeqPlayer.c   the audio-thread side: a fifth sequence player that runs MM's sequence 0
//                            with MM's sequence-script interpreter (2ship2harkinian/mm/src/audio/lib/seqplayer.c).
//   MergeShipMMSfx.c         the game-thread side: MM's sfx request/bank/channel system
//                            (2ship2harkinian/mm/src/audio/sfx.c and the AudioSfx_* parts of code_8019AF00.c),
//                            the player's OoT->MM sound translation and the MM ocarina instruments.

#include "z64.h"

// The MM sequence player's index in audio commands. Ocarina of Time's players are 0..3; commands with
// this player index and an op below 0x80 are routed to MergeShip_MMAudio_ProcessCmd.
#define MERGESHIP_MM_SEQ_PLAYER 4

#ifdef __cplusplus
extern "C" {
#endif

// ---- audio thread (MergeShipMMSeqPlayer.c) --------------------------------------------------------------
extern SequencePlayer gMergeShipMMSeqPlayer;
extern volatile u32 gMergeShipMMAudioResetCount;
// Set by the audio thread when MM's sequence 0 or its fonts could not be loaded.
extern volatile u8 gMergeShipMMAudioUnavailable;

// Called by AudioHeap_Init after Ocarina of Time's players were set up (every audio reset).
void MergeShip_MMAudio_OnHeapInit(void);
// Called by AudioHeap_ResetStep when an audio reset begins (Ocarina of Time's players are disabled there).
void MergeShip_MMAudio_OnResetBegin(void);
// Called by AudioSeq_ProcessSequences after Ocarina of Time's players, before Audio_ProcessNotes.
void MergeShip_MMAudio_ProcessSequence(void);
// Audio_ProcessCmd: a command whose player index is MERGESHIP_MM_SEQ_PLAYER.
void MergeShip_MMAudio_ProcessCmd(AudioCmd* cmd);
// Global mute/unmute commands (0xF1/0xF2) apply to the MM player too.
void MergeShip_MMAudio_SetMuted(s32 muted);

// Playback hooks for MM's per-layer features (no-ops for Ocarina of Time's layers):
// MM AudioPlayback_NoteInitForLayer's startSamplePos (Ocarina of Time's synthesis reads note->unk_BC).
void MergeShip_MMAudio_NoteInitForLayer(Note* note, SequenceLayer* layer);
// MM AudioPlayback_ProcessNotes / SeqLayerDecayRelease: reverb and gain from the layer or the channel.
void MergeShip_MMAudio_LayerAttrs(SequenceLayer* layer, u8* reverb, u8* gain);
// MM AudioEffects_InitVibrato: the channel's or the layer's vibrato.
SequenceChannel* MergeShip_MMAudio_VibratoChannel(SequenceLayer* layer, SequenceChannel* channel);

// ---- resource side (MergeShipAudio.cpp) -----------------------------------------------------------------
// AudioLoad_Init: add MM's sequence-0 sound fonts to fontMap under new font ids.
void MergeShip_MMAudio_RegisterFonts(void);
// MM sequence 0's font list, as Ocarina of Time font ids. Returns the count (0 if unavailable).
s32 MergeShip_MMAudio_GetSeqFonts(u8* outFonts, s32 maxFonts);
// A private, writable copy of MM's sequence 0 (the script writes into itself). NULL if unavailable.
u8* MergeShip_MMAudio_GetSeqData(s32* outSize);

// ---- game thread (MergeShipMMSfx.c) ---------------------------------------------------------------------
// Audio_Update, after Ocarina of Time's active sfx are processed.
void MergeShip_MMSfx_Update(void);
// Audio_ResetSounds.
void MergeShip_MMSfx_Reset(void);
// Audio_StopSfxByPos: an actor is going away; its projected position must not stay referenced.
void MergeShip_MMSfx_StopByPos(Vec3f* pos);
// MM AudioSfx_SetFreqAndStereoBits, seq-0 custom function 0.
u32 MergeShip_MMSfx_SetFreqAndStereoBits(s8 value, SequenceChannel* channel);
u8* MergeShip_MMSfx_GetChannelState(s32 channelIndex);

// MM AudioSfx_PlaySfx and the Audio_PlaySfx_* wrappers the forms use (ids are MM_NA_SE_*).
void MergeShip_MMSfx_PlaySfx(u16 mmSfxId, Vec3f* pos, u8 token, f32* freqScale, f32* volume, s8* reverbAdd);
void MergeShip_MMSfx_PlayAtPosWithSyncedFreqAndVolume(Vec3f* pos, u16 mmSfxId, f32 freqVolParam);
void MergeShip_MMSfx_PlayAtPosForMetalEffectsWithSyncedFreqAndVolume(Vec3f* pos, u16 mmSfxId, f32 freqVolParam);
void MergeShip_MMSfx_PlayAtPosWithAllChannelsIO(Vec3f* pos, u16 mmSfxId, u8 ioData); // MergeShip2: Bremen chime
u8 MergeShip_MMSfx_IsPlaying(u16 mmSfxId);

// The Ocarina of Time sfx id with the same NA_SE_ name in Majora's Mask, or 0 when MM has none.
u16 MergeShip_OoTToMMSfxId(u16 ootSfxId);
// z_actor.c Player_PlaySfx: for a player in an MM form, play the MM sound of the same name.
// Returns true when handled.
s32 MergeShip_TryPlayMMPlayerSfx(Actor* actor, u16 ootSfxId);
// z_player.c Player_ProcessAnimSfxList, for MM forms: the OoT list's entries, played as MM's
// Player_PlayAnimSfx plays each type (ids translated by name). `entries` is z_player.c's AnimSfxEntry*.
void MergeShip_PlayOoTAnimSfxListAsMM(Player* player, const void* entries);
// z_player.c's sound helpers, for MM forms (OoT base id in, MM id + MM's offsets played):
void MergeShip_PlayOoTVoiceSfxAsMM(Player* player, u16 ootSfxId);           // MM Player_AnimSfx_PlayVoice
void MergeShip_StopOoTVoiceSfxAsMM(Player* player, u16 ootSfxId);           // MM func_8082E00C
void MergeShip_PlayOoTFloorSfxAsMM(Player* player, u16 ootSfxId, s32 byAge); // MM Player_AnimSfx_PlayFloor(ByAge)
void MergeShip_PlayMMJumpingSfx(Player* player);                             // MM Player_AnimSfx_PlayFloorJump
void MergeShip_PlayMMLandingSfx(Player* player);                             // MM Player_AnimSfx_PlayFloorLand
void MergeShip_PlayMMFloorWalkSfx(Player* player, f32 freqVolumeLerp);        // MM Player_AnimSfx_PlayFloorWalk

// ---- MM ocarina instruments (MM z_message.c sPlayerFormOcarinaInstruments) ------------------------------
// Records the form the player plays the ocarina in: Deku pipes, Goron drums, Zora guitar.
void MergeShip_SetFormOcarinaInstrument(u8 playerForm);
// AudioOcarina_SetInstrument, after Ocarina of Time recorded the instrument.
void MergeShip_MMOcarina_OnSetInstrument(u8 ootInstrumentId);
// True while notes must go to MM's ocarina channel instead of Ocarina of Time's.
s32 MergeShip_MMOcarina_IsActive(void);
// The ocarina note/vibrato/stop requests, for MM's sequence 0 (ports 7/5/6 of channel 13 and NA_SE_OC_OCARINA).
void MergeShip_MMOcarina_PlayNote(u8 pitch, f32* freqScale, f32* volume);
void MergeShip_MMOcarina_SetVibrato(u8 vibrato);
void MergeShip_MMOcarina_Stop(void);
// Another player's notes (Anchor), on their form's instrument (Deku pipes, Goron drums, Zora guitar).
s32 MergeShip_MMOcarina_FormHasInstrument(u8 playerForm);
void MergeShip_MMOcarina_PlayRemoteNote(u8 playerForm, u8 pitch, Vec3f* pos, f32* freqScale, f32* volume);
void MergeShip_MMOcarina_StopRemoteNote(void);

// ---- the forms' song fanfares (MergeShipAudio.cpp) ------------------------------------------------------
// AudioLoad_Init, after the custom sequences: reserve the sequence ids the forms' fanfares play under.
void MergeShip_MMAudio_ReserveFanfareIds(void);
// MM Audio_PlayFanfareWithPlayerIOPort7(sOcarinaSongFanfares[song], sOcarinaSongFanfareIoData[form]): the song's
// fanfare played with a Deku's, Goron's or Zora's instrument. `song` is Ocarina of Time's OCARINA_SONG_*.
// Returns false when Ocarina of Time's fanfare must be played instead (human forms, warp songs).
s32 MergeShip_PlayFormSongFanfare(u8 song);
// The same for one of MM's own songs (`mmSong` = MergeShipSongs.h song - MERGESHIP_SONG_FIRST), for every form: the
// human plays MM's ocarina (io 0x35). Returns false when mm.o2r cannot provide it.
s32 MergeShip_MMAudio_PlayMMSongFanfare(s32 mmSong);
// Harness evidence: logs the font and instrument the fanfare player's channels play with.
void MergeShip_MMAudio_LogFanfareInstruments(void);
// Singleplayer: MM's Happy Mask Salesman theme (NA_BGM_HAPPY_MASK_SALESMAN) on the main BGM player while the traveling
// salesman talks, and the scene's sequence back after. Returns false when mm.o2r cannot provide it.
s32 MergeShip_MMAudio_PlayHMSTheme(void);
void MergeShip_MMAudio_StopHMSTheme(void);
// MM Audio_PlayFanfare: 0 Bremen March, 1 Kamaro's dance, 2 getting a mask (MergeShipAudio.cpp).
s32 MergeShip_MMAudio_PlayMMFanfare(s32 which);

#ifdef __cplusplus
}
#endif
