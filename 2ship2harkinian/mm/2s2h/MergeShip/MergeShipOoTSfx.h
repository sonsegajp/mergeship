#pragma once

// MergeShip2 -- Ocarina of Time's sound effects in Majora's Mask (2Ship), for what Termina borrows from Hyrule
// (Ganondorf mode: his voice, his magic). The ids are OoT's (MergeShipOoTSfxIds.h: OOT_NA_SE_*), not MM's: Ocarina of
// Time's sfx system, ported (MergeShipOoTSfx.c), plays them on a sequence player of their own.

#include "ultra64.h"
#include "MergeShipOoTSfxIds.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct PlayState;

// At the actor (as Audio_PlayActorSound2); the Loop forms are sounds held while called each frame (OoT's - SFX_FLAG).
void MergeShip_OoTSfx_Actor(struct Actor* actor, u16 ootSfxId);
void MergeShip_OoTSfx_ActorLoop(struct Actor* actor, u16 ootSfxId);
// Centred (as Sfx_PlaySfxCentered).
void MergeShip_OoTSfx_Centered(u16 ootSfxId);
void MergeShip_OoTSfx_CenteredLoop(u16 ootSfxId);
// At a fixed world position for `duration` frames (as SoundSource_PlaySfxAtFixedWorldPos).
void MergeShip_OoTSfx_AtPos(struct PlayState* play, Vec3f* pos, s32 duration, u16 ootSfxId);
// The sounds at this (projected) position stopped: its actor is going away (Actor_Delete, as Audio_StopSfxByPos).
void MergeShip_OoTSfx_StopByPos(Vec3f* pos);
// Once a game frame (MM's Audio_Update): requests to sounds, on Ocarina of Time's sequence 0 on its own player.
void MergeShip_OoTSfx_AudioUpdate(void);

#ifdef __cplusplus
}
#endif
