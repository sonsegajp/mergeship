// MergeShip2 -- the Bremen Mask's followers: the shared pieces Ocarina of Time's dogs (ovl_En_Dog) and cuccos
// (ovl_En_Niw) use to follow a marching Link as Majora's Mask's dogs (ovl_En_Dg) and chicks (ovl_En_Nwc) do.
// Ported from 2ship2harkinian/mm: src/overlays/actors/ovl_En_Nwc/z_en_nwc.c, ovl_En_Hs/z_en_hs.c and
// src/audio/code_8019AF00.c.

#include "global.h"
#include "soh/MergeShip/MergeShipBremen.h"
#include "soh/MergeShip/MergeShipTransformFx.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

// MM sequence.h SEQ_IO_VAL_NONE
#define MERGESHIP_SEQ_IO_VAL_NONE -1

s32 MergeShip_BremenMarching(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return (player != NULL) && (player->stateFlags3 & PLAYER_STATE3_20000000) &&
           CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.BremenFollowers"), 1);
}

// MM func_801A3950(SEQ_PLAYER_FANFARE, true)
static u8 MergeShip_Bremen_ReadFanfareChannelIO(void) {
    u8 ret = (u8)MERGESHIP_SEQ_IO_VAL_NONE;
    SequenceChannel* channel = gAudioContext.seqPlayers[SEQ_PLAYER_FANFARE].channels[15];

    if (gAudioContext.seqPlayers[SEQ_PLAYER_FANFARE].enabled && (channel != NULL) && channel->enabled) {
        if (channel->soundScriptIO[0] != MERGESHIP_SEQ_IO_VAL_NONE) {
            ret = channel->soundScriptIO[0];
            // MM SEQCMD_SET_CHANNEL_IO(seqPlayerIndex, 15, 0, (u8)SEQ_IO_VAL_NONE): Ocarina of Time's seq command 0x8.
            Audio_QueueSeqCmd((0x8 << 28) | ((u8)SEQ_PLAYER_FANFARE << 24) | (0 << 16) | (15 << 8) |
                              (u8)MERGESHIP_SEQ_IO_VAL_NONE);
        }
    }
    return ret;
}

// MM func_801A46F8
s32 MergeShip_Bremen_OnMarchBeat(void) {
    // Not in MM: the march's fanfare did not start (no mm.o2r sequence), so there is no beat to wait for.
    if (!gAudioContext.seqPlayers[SEQ_PLAYER_FANFARE].enabled) {
        return true;
    }

    switch (MergeShip_Bremen_ReadFanfareChannelIO()) {
        case 0:
        case 0x8:
        case 0x10:
        case 0x18:
        case 0x20:
            return true;

        default:
            return false;
    }
}

// ---- MM Grog's chick chain -------------------------------------------------------------------------------------

static Vec3f sNwcPos[MERGESHIP_BREMEN_CHAIN_COUNT]; // MM EnHs::nwcPos
static s16 sChickCount = 0;                          // MM EnHs::actor.home.rot.z (followers in line, by 2)
static s16 sAdultCount = 0;                          // MM EnHs::actor.home.rot.x (never grows here, see below)
static s32 sChainValid = false;
static s16 sChainScene = -1;
static u32 sChainFrame = 0;

// MM func_80952C50 (EnHs_Init)
static void MergeShip_Bremen_InitChain(PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sNwcPos); i++) {
        Math_Vec3f_Copy(&sNwcPos[i], &player->actor.world.pos);
    }

    sAdultCount = 0; // reset adult transformed count
    sChickCount = 0; // reset chick count
}

void MergeShip_Bremen_ResetChain(void) {
    sChainValid = false;
}

// MM EnHs_UpdateChickPos
static void MergeShip_Bremen_UpdateChickPos(Vec3f* dst, Vec3f src, f32 offset) {
    Vec3f diff;
    f32 distance;

    Math_Vec3f_Diff(&src, dst, &diff);

    distance = SQXZ(diff); // gets un-squared after we check if we are too close

    if (SQ(offset) > distance) {
        return;
    }

    distance = sqrtf(distance);
    diff.x *= (distance - offset) / distance;
    diff.z *= (distance - offset) / distance;

    dst->x += diff.x;
    dst->z += diff.z;
}

// MM func_80952F00 (called from EnHs_Update every frame)
void MergeShip_Bremen_UpdateChain(PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 i;
    f32 offset;

    if (player == NULL) {
        return;
    }

    // A new scene is MM's EnHs_Init.
    if (!sChainValid || (sChainScene != play->sceneNum)) {
        sChainValid = true;
        sChainScene = play->sceneNum;
        sChainFrame = play->gameplayFrames;
        MergeShip_Bremen_InitChain(play);
    } else if (sChainFrame == play->gameplayFrames) {
        return; // Grog is a single actor: once per frame.
    }
    sChainFrame = play->gameplayFrames;

    // Not in MM: nobody can be in line once the march is over, even if no follower was left to reset the count
    // (MM's chicks cannot be carried off; Hyrule's cuccos can).
    if (!MergeShip_BremenMarching(play)) {
        sChickCount = 0;
        sAdultCount = 0;
    }

    // MM spaces the line 15 units apart once all ten chicks are in it (they then grow into cuccos) and 10 before.
    // Hyrule's followers are grown cuccos from the start, so they always keep the grown-up spacing.
    offset = 15.0f;

    MergeShip_Bremen_UpdateChickPos(&sNwcPos[0], player->actor.world.pos, offset);

    for (i = 1; i < ARRAY_COUNT(sNwcPos); i++) {
        MergeShip_Bremen_UpdateChickPos(&sNwcPos[i], sNwcPos[i - 1], offset);
    }
}

Vec3f* MergeShip_Bremen_GetChain(void) {
    return sNwcPos;
}

s16 MergeShip_Bremen_GetFollowCount(void) {
    return sChickCount;
}

// ---- MM EnNwc's helpers ----------------------------------------------------------------------------------------

// MM EnNwc_PlayerReleasedBremanMarch
s32 MergeShip_Bremen_PlayerReleasedMarch(PlayState* play) {
    // Weird: home.rot.x holds count of chicks having transformed into adult.
    // (Hyrule's cuccos are grown already and never transform, so this stays 0.)
    if (sAdultCount >= 20) {
        return false;
    }

    if (MergeShip_BremenMarching(play)) {
        return false;
    }

    return true;
}

// MM EnNwc_IsFound
s32 MergeShip_Bremen_IsFound(Actor* actor, PlayState* play) {
    // MM: `if (this->grog == NULL) return false;` -- the chain above stands in for Grog.
    // Not in MM: the line holds MM's ten followers; an eleventh would index past MM's nwcPos[20].
    if (sChickCount >= 20) {
        return false;
    }

    if (MergeShip_BremenMarching(play) && (actor->xzDistToPlayer < 100.0f)) {
        return true;
    }

    return false;
}

// MM EnNwc_CheckFound, once EnNwc_IsFound
s16 MergeShip_Bremen_JoinLine(void) {
    u8 currentChickCount = (sChickCount / 2);
    s16 order;

    if (currentChickCount > 9) {
        currentChickCount = 9;
    }

    // save our current chick order
    order = sChickCount + 1;

    // if < 10 chicks, increment grog's chick counter
    if (sChickCount < 20) {
        sChickCount += 2;
    }

    MergeShip_MMSfx_PlayAtPosWithAllChannelsIO(&gSfxDefaultPos, MM_NA_SE_SY_CHICK_JOIN_CHIME, currentChickCount);
    return order;
}

// MM EnNwc_Follow, once EnNwc_PlayerReleasedBremanMarch
void MergeShip_Bremen_ResetLine(void) {
    sAdultCount = 0; // reset adult count
    sChickCount = 0; // reset chick follow count
}
