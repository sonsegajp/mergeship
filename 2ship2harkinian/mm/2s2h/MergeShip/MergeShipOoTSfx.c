// MergeShip2 -- Ocarina of Time's sound effects in Majora's Mask (MergeShipOoTSfx.h).
//
// Ocarina of Time's sfx system, ported from SoH src/code/code_800F7260.c (requests, banks, active sounds) and the
// Audio_SetSfxProperties part of src/code/code_800EC960.c (volume, reverb, pan, frequency and filter from the sound's
// projected position), with its per-sound parameters (MergeShipOoTSfxParams.inc). It is a second, independent copy of
// the system: its banks, requests and channel state are its own, and it drives Ocarina of Time's sequence 0 (the sfx
// sequence, registered with its fonts by MergeShipHyrule.cpp) on a sixth sequence player of MM's audio library,
// MERGESHIP_OOT_SFX_SEQ_PLAYER (MM's interpreter runs it: it is a superset of OoT's, but for the two commands it reads
// as Ocarina of Time does on that player -- seqplayer.c). Majora's Mask's own sfx system, its banks and its sequence
// player 2 are untouched. (SoH does the reverse for MM's sounds: soh/MergeShip/MergeShipMMSfx.c.)
//
// Scene-level state OoT's formulas read (environment / spec reverb, the filters, the sound output mode) is MM's, which
// keeps the same variables for the same purpose (code_8019AF00.c).

#include "MergeShipOoTSfx.h"
#include "global.h"

#include <math.h>

void MergeShipTest_Log(const char* fmt, ...);
u16 MergeShip_Audio_OoTSeqId(u16 ootSeqId); // MergeShipHyrule.cpp

// MM's (code_8019AF00.c): the same variables as OoT's for the same purpose
extern s8 sSoundMode;
extern s8 sSpecReverb;
extern s8 sAudioEnvReverb;
extern s8 sAudioCodeReverb;
extern u8 sAudioBaseFilter;
extern u8 sAudioExtraFilter;
extern u8 sAudioBaseFilter2;
extern u8 sAudioExtraFilter2;

#define OOT_PLAYER MERGESHIP_OOT_SFX_SEQ_PLAYER

// ---- OoT z64audio.h --------------------------------------------------------------------------------------------------

#define OOT_SFX_BANK_SHIFT(sfxId) (((sfxId) >> 12) & 0xFF)
#define OOT_SFX_BANK_MASK(sfxId) ((sfxId)&0xF000)
#define OOT_SFX_INDEX(sfxId) ((sfxId)&0x01FF)
#define OOT_SFX_BANK(sfxId) OOT_SFX_BANK_SHIFT(OOT_SFX_BANK_MASK(sfxId))
#define OOT_SFX_FLAG 0x800

#define OOT_MAX_CHANNELS_PER_BANK 3

typedef enum {
    OOT_BANK_PLAYER,
    OOT_BANK_ITEM,
    OOT_BANK_ENV,
    OOT_BANK_ENEMY,
    OOT_BANK_SYSTEM,
    OOT_BANK_OCARINA,
    OOT_BANK_VOICE
} OoTSoundBankId;

typedef enum {
    OOT_SFX_STATE_EMPTY,
    OOT_SFX_STATE_QUEUED,
    OOT_SFX_STATE_READY,
    OOT_SFX_STATE_PLAYING_REFRESH,
    OOT_SFX_STATE_PLAYING_1,
    OOT_SFX_STATE_PLAYING_2
} OoTSfxState;

typedef struct {
    f32* posX;
    f32* posY;
    f32* posZ;
    u8 token;
    f32* freqScale;
    f32* vol;
    s8* reverbAdd;
    f32 dist;
    u32 priority; // lower is more prioritized
    u8 sfxImportance;
    u16 sfxParams;
    u16 sfxId;
    u8 state; // OoTSfxState
    u8 freshness;
    u8 prev;
    u8 next;
    u8 channelIdx;
    u8 unk_2F;
} OoTSoundBankEntry;

typedef struct {
    u32 priority;
    u8 entryIndex;
} OoTActiveSound;

typedef struct {
    u8 importance;
    u16 params;
} OoTSoundParams;

typedef struct {
    f32 vol;
    f32 freqScale;
    s8 reverb;
    s8 pan;
    s8 stereoBits;
    u8 filter;
    u8 unk_0C;
} OoTSfxChannelState;

typedef struct {
    u16 sfxId;
    Vec3f* pos;
    u8 token;
    f32* freqScale;
    f32* vol;
    s8* reverbAdd;
} OoTSoundRequest;

#include "MergeShipOoTSfxParams.inc"

// ---- OoT code_800EC960.c's tables ------------------------------------------------------------------------------------

static u8 sIsLargeSoundBank[7] = { 0, 0, 0, 1, 0, 0, 0 };
// Only the first row is supported by sequence 0 (gSfxChannelLayout is always 0).
static u8 sChannelsPerBank[7] = { 3, 2, 3, 3, 2, 1, 2 };
static u8 sUsedChannelsPerBank[7] = { 3, 2, 3, 2, 2, 1, 1 };
static f32 sBehindScreenZ[2] = { -15.0f, -65.0f };

// ---- OoT code_800F7260.c's state -------------------------------------------------------------------------------------

static OoTSoundBankEntry sBank0[9];
static OoTSoundBankEntry sBank1[12];
static OoTSoundBankEntry sBank2[22];
static OoTSoundBankEntry sBank3[20];
static OoTSoundBankEntry sBank4[8];
static OoTSoundBankEntry sBank5[3];
static OoTSoundBankEntry sBank6[5];
static OoTSoundBankEntry* sSoundBanks[7] = { sBank0, sBank1, sBank2, sBank3, sBank4, sBank5, sBank6 };
static u8 sBankSizes[7] = { ARRAY_COUNT(sBank0), ARRAY_COUNT(sBank1), ARRAY_COUNT(sBank2), ARRAY_COUNT(sBank3),
                            ARRAY_COUNT(sBank4), ARRAY_COUNT(sBank5), ARRAY_COUNT(sBank6) };
static OoTSoundRequest sSoundRequests[0x100];
static u8 sSoundBankListEnd[7];
static u8 sSoundBankFreeListStart[7];
static OoTActiveSound sActiveSounds[7][OOT_MAX_CHANNELS_PER_BANK];
static u8 sCurSfxPlayerChannelIdx;
static OoTSfxChannelState sSfxChannelState[0x10];
static u8 sSoundRequestWriteIndex = 0;
static u8 sSoundRequestReadIndex = 0;

static Vec3f sSfxDefaultPos = { 0.0f, 0.0f, 0.0f };
static f32 sSfxDefaultFreqAndVolScale = 1.0f;
static s8 sSfxDefaultReverb = 0;

// The sequence player: started once MM's audio is up (and again after an audio reset disables it).
static s32 sResetDone = false;
static s32 sStartCooldown = 0;

static void OoTSfx_ResetSfxChannelState(void) {
    u8 i;

    for (i = 0; i < 16; i++) {
        sSfxChannelState[i].vol = 1.0f;
        sSfxChannelState[i].freqScale = 1.0f;
        sSfxChannelState[i].reverb = 0;
        sSfxChannelState[i].pan = 0x40;
        sSfxChannelState[i].stereoBits = 0;
        sSfxChannelState[i].filter = 0xFF;
        sSfxChannelState[i].unk_0C = 0xFF;
    }
}

static void OoTSfx_ResetSounds(void) {
    u8 bankId;
    u8 i;

    sSoundRequestWriteIndex = 0;
    sSoundRequestReadIndex = 0;
    for (bankId = 0; bankId < ARRAY_COUNT(sSoundBanks); bankId++) {
        sSoundBankListEnd[bankId] = 0;
        sSoundBankFreeListStart[bankId] = 1;
        for (i = 0; i < OOT_MAX_CHANNELS_PER_BANK; i++) {
            sActiveSounds[bankId][i].entryIndex = 0xFF;
        }
        sSoundBanks[bankId][0].prev = 0xFF;
        sSoundBanks[bankId][0].next = 0xFF;
        for (i = 1; i < sBankSizes[bankId] - 1; i++) {
            sSoundBanks[bankId][i].prev = i - 1;
            sSoundBanks[bankId][i].next = i + 1;
        }
        sSoundBanks[bankId][i].prev = i - 1;
        sSoundBanks[bankId][i].next = 0xFF;
    }
    OoTSfx_ResetSfxChannelState();
    sResetDone = true;
}

// Audio_PlaySfxGeneral
static void OoTSfx_PlayGeneral(u16 sfxId, Vec3f* pos, u8 token, f32* freqScale, f32* vol, s8* reverbAdd) {
    OoTSoundRequest* req;

    if (!sResetDone) {
        OoTSfx_ResetSounds();
    }
    if (OOT_SFX_BANK(sfxId) >= ARRAY_COUNT(sSoundBanks)) {
        return;
    }
    req = &sSoundRequests[sSoundRequestWriteIndex];
    req->sfxId = sfxId;
    req->pos = pos;
    req->token = token;
    req->freqScale = freqScale;
    req->vol = vol;
    req->reverbAdd = reverbAdd;
    sSoundRequestWriteIndex++;
}

static void OoTSfx_RemoveMatchingSoundRequests(u8 aspect, OoTSoundBankEntry* cmp) {
    OoTSoundRequest* req;
    s32 remove;
    u8 i = sSoundRequestReadIndex;

    for (; i != sSoundRequestWriteIndex; i++) {
        remove = false;
        req = &sSoundRequests[i];
        switch (aspect) {
            case 0:
                if (OOT_SFX_BANK_MASK(req->sfxId) == OOT_SFX_BANK_MASK(cmp->sfxId)) {
                    remove = true;
                }
                break;
            case 1:
                if ((OOT_SFX_BANK_MASK(req->sfxId) == OOT_SFX_BANK_MASK(cmp->sfxId)) && (&req->pos->x == cmp->posX)) {
                    remove = true;
                }
                break;
            case 2:
                if (&req->pos->x == cmp->posX) {
                    remove = true;
                }
                break;
            case 3:
                if ((&req->pos->x == cmp->posX) && (req->sfxId == cmp->sfxId)) {
                    remove = true;
                }
                break;
            case 4:
                if ((req->token == cmp->token) && (req->sfxId == cmp->sfxId)) {
                    remove = true;
                }
                break;
            case 5:
                if (req->sfxId == cmp->sfxId) {
                    remove = true;
                }
                break;
        }
        if (remove) {
            req->sfxId = 0;
        }
    }
}

static OoTSoundParams* OoTSfx_Params(u16 sfxId) {
    return &sOoTSoundParams[OOT_SFX_BANK_SHIFT(sfxId)][OOT_SFX_INDEX(sfxId)];
}

// Audio_ProcessSoundRequest
static void OoTSfx_ProcessSoundRequest(void) {
    u16 sfxId;
    u8 count;
    u8 index;
    OoTSoundRequest* req;
    OoTSoundBankEntry* entry;
    OoTSoundParams* soundParams;
    s32 bankId;
    u8 evictImportance = 0;
    u8 evictIndex;

    req = &sSoundRequests[sSoundRequestReadIndex];
    evictIndex = 0x80;
    if (req->sfxId == 0) {
        return;
    }
    bankId = OOT_SFX_BANK(req->sfxId);
    count = 0;
    index = sSoundBanks[bankId][0].next;
    while ((index != 0xFF) && (index != 0)) {
        if (sSoundBanks[bankId][index].posX == &req->pos->x) {
            if ((OoTSfx_Params(req->sfxId)->params & 0x20) &&
                (OoTSfx_Params(req->sfxId)->importance == sSoundBanks[bankId][index].sfxImportance)) {
                return;
            }
            if (sSoundBanks[bankId][index].sfxId == req->sfxId) {
                count = sUsedChannelsPerBank[bankId];
            } else {
                if (count == 0) {
                    evictIndex = index;
                    sfxId = sSoundBanks[bankId][index].sfxId & 0xFFFF;
                    evictImportance = OoTSfx_Params(sfxId)->importance;
                } else if (sSoundBanks[bankId][index].sfxImportance < evictImportance) {
                    evictIndex = index;
                    sfxId = sSoundBanks[bankId][index].sfxId & 0xFFFF;
                    evictImportance = OoTSfx_Params(sfxId)->importance;
                }
                count++;
                if (count == sUsedChannelsPerBank[bankId]) {
                    if (OoTSfx_Params(req->sfxId)->importance >= evictImportance) {
                        index = evictIndex;
                    } else {
                        index = 0;
                    }
                }
            }
            if (count == sUsedChannelsPerBank[bankId]) {
                soundParams = OoTSfx_Params(req->sfxId);
                if ((req->sfxId & 0xC00) || (soundParams->params & 4) || (index == evictIndex)) {
                    sSoundBanks[bankId][index].token = req->token;
                    sSoundBanks[bankId][index].sfxId = req->sfxId;
                    sSoundBanks[bankId][index].state = OOT_SFX_STATE_QUEUED;
                    sSoundBanks[bankId][index].freshness = 2;
                    sSoundBanks[bankId][index].freqScale = req->freqScale;
                    sSoundBanks[bankId][index].vol = req->vol;
                    sSoundBanks[bankId][index].reverbAdd = req->reverbAdd;
                    sSoundBanks[bankId][index].sfxParams = soundParams->params;
                    sSoundBanks[bankId][index].sfxImportance = soundParams->importance;
                } else if (sSoundBanks[bankId][index].state == OOT_SFX_STATE_PLAYING_2) {
                    sSoundBanks[bankId][index].state = OOT_SFX_STATE_PLAYING_1;
                }
                index = 0;
            }
        }
        if (index != 0) {
            index = sSoundBanks[bankId][index].next;
        }
    }
    if ((sSoundBanks[bankId][sSoundBankFreeListStart[bankId]].next != 0xFF) && (index != 0)) {
        index = sSoundBankFreeListStart[bankId];
        entry = &sSoundBanks[bankId][index];
        entry->posX = &req->pos->x;
        entry->posY = &req->pos->y;
        entry->posZ = &req->pos->z;
        entry->token = req->token;
        entry->freqScale = req->freqScale;
        entry->vol = req->vol;
        entry->reverbAdd = req->reverbAdd;
        soundParams = OoTSfx_Params(req->sfxId);
        entry->sfxParams = soundParams->params;
        entry->sfxImportance = soundParams->importance;
        entry->sfxId = req->sfxId;
        entry->state = OOT_SFX_STATE_QUEUED;
        entry->freshness = 2;
        entry->prev = sSoundBankListEnd[bankId];
        sSoundBanks[bankId][sSoundBankListEnd[bankId]].next = sSoundBankFreeListStart[bankId];
        sSoundBankListEnd[bankId] = sSoundBankFreeListStart[bankId];
        sSoundBankFreeListStart[bankId] = sSoundBanks[bankId][sSoundBankFreeListStart[bankId]].next;
        sSoundBanks[bankId][sSoundBankFreeListStart[bankId]].prev = 0xFF;
        entry->next = 0xFF;
    }
}

static void OoTSfx_RemoveSoundBankEntry(u8 bankId, u8 entryIndex) {
    OoTSoundBankEntry* entry = &sSoundBanks[bankId][entryIndex];
    u8 i;

    if (entryIndex == sSoundBankListEnd[bankId]) {
        sSoundBankListEnd[bankId] = entry->prev;
    } else {
        sSoundBanks[bankId][entry->next].prev = entry->prev;
    }
    sSoundBanks[bankId][entry->prev].next = entry->next;
    entry->next = sSoundBankFreeListStart[bankId];
    entry->prev = 0xFF;
    sSoundBanks[bankId][sSoundBankFreeListStart[bankId]].prev = entryIndex;
    sSoundBankFreeListStart[bankId] = entryIndex;
    entry->state = OOT_SFX_STATE_EMPTY;

    for (i = 0; i < sChannelsPerBank[bankId]; i++) {
        if (sActiveSounds[bankId][i].entryIndex == entryIndex) {
            sActiveSounds[bankId][i].entryIndex = 0xFF;
            i = sChannelsPerBank[bankId];
        }
    }
}

static void OoTSfx_StopChannel(u8 channelIdx) {
    AUDIOCMD_CHANNEL_SET_IO(OOT_PLAYER, channelIdx, 0, 0);
}

// Audio_ChooseActiveSounds
static void OoTSfx_ChooseActiveSounds(u8 bankId) {
    u8 numChosenSounds;
    u8 numChannels;
    u8 entryIndex;
    u8 i;
    u8 j;
    u8 k;
    u8 sfxImportance;
    u8 needNewSound;
    u8 chosenEntryIndex;
    OoTSoundBankEntry* entry;
    OoTActiveSound chosenSounds[OOT_MAX_CHANNELS_PER_BANK];
    OoTActiveSound* activeSound;

    numChosenSounds = 0;
    for (i = 0; i < OOT_MAX_CHANNELS_PER_BANK; i++) {
        chosenSounds[i].priority = 0x7FFFFFFF;
        chosenSounds[i].entryIndex = 0xFF;
    }
    entryIndex = sSoundBanks[bankId][0].next;
    k = 0;
    while (entryIndex != 0xFF) {
        if ((sSoundBanks[bankId][entryIndex].state == OOT_SFX_STATE_QUEUED) &&
            (sSoundBanks[bankId][entryIndex].sfxId & 0xC00)) {
            sSoundBanks[bankId][entryIndex].freshness--;
        } else if (!(sSoundBanks[bankId][entryIndex].sfxId & 0xC00) &&
                   (sSoundBanks[bankId][entryIndex].state == OOT_SFX_STATE_PLAYING_2)) {
            OoTSfx_StopChannel(sSoundBanks[bankId][entryIndex].channelIdx);
            OoTSfx_RemoveSoundBankEntry(bankId, entryIndex);
        }
        if (sSoundBanks[bankId][entryIndex].freshness == 0) {
            OoTSfx_RemoveSoundBankEntry(bankId, entryIndex);
        } else if (sSoundBanks[bankId][entryIndex].state != OOT_SFX_STATE_EMPTY) {
            entry = &sSoundBanks[bankId][entryIndex];

            if (&sSfxDefaultPos.x == entry[0].posX) {
                entry->dist = 0.0f;
            } else {
                entry->dist = SQ(*entry->posX) + SQ(*entry->posY) + SQ(*entry->posZ);
            }
            sfxImportance = entry->sfxImportance;
            if (entry->sfxParams & 0x10) {
                entry->priority = SQ(0xFF - sfxImportance) * SQ(76);
            } else {
                if (entry->dist > 0x7FFFFFD0) {
                    entry->dist = 0x70000008;
                }
                entry->priority = (u32)entry->dist + (SQ(0xFF - sfxImportance) * SQ(76));
                if (*entry->posZ < 0.0f) {
                    entry->priority += (s32)(-*entry->posZ * 6.0f);
                }
            }
            if (entry->dist > SQ(1e5f)) {
                if (entry->state == OOT_SFX_STATE_PLAYING_1) {
                    OoTSfx_StopChannel(entry->channelIdx);
                    if (entry->sfxId & 0xC00) {
                        OoTSfx_RemoveSoundBankEntry(bankId, entryIndex);
                        entryIndex = k;
                    }
                }
            } else {
                numChannels = sChannelsPerBank[bankId];
                for (i = 0; i < numChannels; i++) {
                    if (chosenSounds[i].priority >= entry->priority) {
                        if (numChosenSounds < sChannelsPerBank[bankId]) {
                            numChosenSounds++;
                        }
                        for (j = numChannels - 1; j > i; j--) {
                            chosenSounds[j].priority = chosenSounds[j - 1].priority;
                            chosenSounds[j].entryIndex = chosenSounds[j - 1].entryIndex;
                        }
                        chosenSounds[i].priority = entry->priority;
                        chosenSounds[i].entryIndex = entryIndex;
                        i = numChannels; // "break;"
                    }
                }
            }
            k = entryIndex;
        }
        entryIndex = sSoundBanks[bankId][k].next;
    }
    for (i = 0; i < numChosenSounds; i++) {
        entry = &sSoundBanks[bankId][chosenSounds[i].entryIndex];
        if (entry->state == OOT_SFX_STATE_QUEUED) {
            entry->state = OOT_SFX_STATE_READY;
        } else if (entry->state == OOT_SFX_STATE_PLAYING_1) {
            entry->state = OOT_SFX_STATE_PLAYING_REFRESH;
        }
    }

    // Pick something to play for all channels.
    numChannels = sChannelsPerBank[bankId];
    for (i = 0; i < numChannels; i++) {
        needNewSound = false;
        activeSound = &sActiveSounds[bankId][i];

        if (activeSound->entryIndex == 0xFF) {
            needNewSound = true;
        } else {
            entry = &sSoundBanks[bankId][activeSound[0].entryIndex];
            if (entry->state == OOT_SFX_STATE_PLAYING_1) {
                if (entry->sfxId & 0xC00) {
                    OoTSfx_RemoveSoundBankEntry(bankId, activeSound->entryIndex);
                } else {
                    entry->state = OOT_SFX_STATE_QUEUED;
                }
                needNewSound = true;
            } else if (entry->state == OOT_SFX_STATE_EMPTY) {
                activeSound->entryIndex = 0xFF;
                needNewSound = true;
            } else {
                // Sound is already playing as it should, nothing to do.
                for (j = 0; j < numChannels; j++) {
                    if (activeSound->entryIndex == chosenSounds[j].entryIndex) {
                        chosenSounds[j].entryIndex = 0xFF;
                        j = numChannels;
                    }
                }
                numChosenSounds--;
            }
        }

        if (needNewSound == true) {
            for (j = 0; j < numChannels; j++) {
                chosenEntryIndex = chosenSounds[j].entryIndex;
                if ((chosenEntryIndex != 0xFF) &&
                    (sSoundBanks[bankId][chosenEntryIndex].state != OOT_SFX_STATE_PLAYING_REFRESH)) {
                    for (k = 0; k < numChannels; k++) {
                        if (chosenEntryIndex == sActiveSounds[bankId][k].entryIndex) {
                            needNewSound = false;
                            k = numChannels; // "break;"
                        }
                    }
                    if (needNewSound == true) {
                        activeSound->entryIndex = chosenEntryIndex;
                        chosenSounds[j].entryIndex = 0xFF;
                        j = numChannels + 1;
                        numChosenSounds--;
                    }
                }
            }
            if (j == numChannels) {
                // nothing found
                activeSound->entryIndex = 0xFF;
            }
        }
    }
}

// ---- Audio_SetSfxProperties and its helpers (code_800EC960.c) --------------------------------------------------------

static f32 OoTSfx_ComputeSoundVolume(u8 bankId, u8 entryIdx) {
    OoTSoundBankEntry* bankEntry = &sSoundBanks[bankId][entryIdx];
    f32 minDist;
    f32 baseDist;
    f32 ret;

    if (bankEntry->sfxParams & 0x2000) {
        return 1.0f;
    }
    if (bankEntry->dist > 10000.0f) {
        ret = 0.0f;
    } else {
        switch (bankEntry->sfxParams & 3) {
            case 1:
                baseDist = 10000.0f / 15.0f;
                break;
            case 2:
                baseDist = 10000.0f / 10.5f;
                break;
            case 3:
                baseDist = 10000.0f / 2.6f;
                break;
            default:
                baseDist = 10000.0f / 20.0f;
                break;
        }
        minDist = baseDist / 5.0f;
        if (bankEntry->dist < minDist) {
            ret = 1.0f;
        } else if (bankEntry->dist < baseDist) {
            ret = ((((baseDist - minDist) - (bankEntry->dist - minDist)) / (baseDist - minDist)) * 0.19f) + 0.81f;
        } else {
            ret = (1.0f - ((bankEntry->dist - baseDist) / (10000.0f - baseDist))) * 0.81f;
        }
        ret = SQ(ret);
    }
    return ret;
}

static s8 OoTSfx_ComputeSoundReverb(u8 bankId, u8 entryIdx, u8 channelIdx) {
    s8 distAdd = 0;
    s32 scriptAdd = 0;
    OoTSoundBankEntry* entry = &sSoundBanks[bankId][entryIdx];
    SequenceChannel* channel = gAudioCtx.seqPlayers[OOT_PLAYER].channels[channelIdx];
    s32 reverb;

    if (!(entry->sfxParams & 0x1000)) {
        if (entry->dist < 2500.0f) {
            distAdd = (*entry->posZ > 0.0f) ? (entry->dist / 2500.0f) * 70.0f : (entry->dist / 2500.0f) * 91.0f;
        } else {
            distAdd = 70;
        }
    }
    if (IS_SEQUENCE_CHANNEL_VALID(channel)) {
        scriptAdd = channel->seqScriptIO[1];
        if (channel->seqScriptIO[1] < 0) {
            scriptAdd = 0;
        }
    }
    reverb = *entry->reverbAdd + distAdd + scriptAdd;
    if ((bankId != OOT_BANK_OCARINA) || !((entry->sfxId & 0x1FF) < 2)) {
        reverb += sAudioEnvReverb + sAudioCodeReverb + sSpecReverb;
    }
    if (reverb > 0x7F) {
        reverb = 0x7F;
    }
    return reverb;
}

static s8 OoTSfx_ComputeSoundPanSigned(f32 x, f32 z, u8 token) {
    f32 absX = (x < 0) ? -x : x;
    f32 absZ = (z < 0) ? -z : z;
    f32 pan;

    if (absX > 8000.0f) {
        absX = 8000.0f;
    }
    if (absZ > 8000.0f) {
        absZ = 8000.0f;
    }
    if ((x == 0.0f) && (z == 0.0f)) {
        pan = 0.5f;
    } else if (absZ <= absX) {
        pan = (16000.0f - absX) / (3.3f * (16000.0f - absZ));
        if (x >= 0.0f) {
            pan = 1.0f - pan;
        }
    } else {
        pan = (x / (5.0769234f * absZ)) + 0.5f; // about 66 / 13
    }
    if ((absZ < 50.0f) && (absX < 50.0f)) {
        pan = ((pan - 0.5f) * SQ(absX / 50.0f)) + 0.5f;
    }
    return (s8)((pan * 127.0f) + 0.5f);
}

static f32 OoTSfx_ComputeSoundFreqScale(u8 bankId, u8 entryIdx) {
    s32 filtered = 0;
    OoTSoundBankEntry* entry = &sSoundBanks[bankId][entryIdx];
    f32 dist;
    f32 freq = 1.0f;

    if (entry->sfxParams & 0x4000) {
        freq = 1.0f - ((gAudioCtx.audioRandom & 0xF) / 192.0f);
    }
    switch (bankId) {
        case OOT_BANK_PLAYER:
        case OOT_BANK_ITEM:
        case OOT_BANK_VOICE:
            if (sAudioBaseFilter2 != 0) {
                filtered = 1;
            }
            break;
        case OOT_BANK_ENV:
        case OOT_BANK_ENEMY:
            if (sAudioExtraFilter2 != 0) {
                filtered = 1;
            }
            break;
        default:
            break;
    }
    if ((filtered == 1) && !(entry->sfxParams & 0x800)) {
        freq *= (1.0293f - ((gAudioCtx.audioRandom & 0xF) / 144.0f));
    }
    dist = entry->dist;
    if (!(entry->sfxParams & 0x2000) && !(entry->sfxParams & 0x8000)) {
        if (dist >= 10000.0f) {
            freq += 0.2f;
        } else {
            freq += (0.2f * (dist / 10000.0f));
        }
    }
    if (entry->sfxParams & 0xC0) {
        freq += (entry->unk_2F / 192.0f);
    }
    return freq;
}

static u8 OoTSfx_ComputeBehindScreenFilter(f32 behindScreenZ, OoTSoundBankEntry* entry, s8 pan) {
    s8 side;
    u8 filter;
    f32 scale;
    f32 dist;

    if (*entry->posZ < behindScreenZ) {
        side = (pan < 65) ? pan : 0x7F - pan;
        if (side < 30) {
            filter = 0;
        } else {
            filter = (((side & 0xFFFF) * 10) - 300) / 34;
            if (filter != 0) {
                filter = 0x10 - filter;
            }
        }
    } else {
        filter = 0;
    }
    if ((filter == 0) && (entry->sfxParams & 0x200)) {
        filter = 0xF;
    }
    switch (entry->sfxParams & 3) {
        case 1:
            scale = 12.0f;
            break;
        case 2:
            scale = 9.0f;
            break;
        case 3:
            scale = 6.0f;
            break;
        default:
            scale = 15.0f;
            break;
    }
    dist = CLAMP_MAX(entry->dist, 10000.0f / 5.2f);
    return (filter * 0x10) + (u8)((scale * dist) / (10000.0f / 5.2f));
}

static s8 OoTSfx_ComputeHeightCombFilter(f32 y, u16 sfxParams) {
    s8 ret = 0;

    if (y >= 0.0f) {
        if (y > 625.0f) {
            ret = 127;
        } else {
            ret = (y / 625.0f) * 126.0f;
        }
    }
    return ret | 1;
}

static void OoTSfx_SetSfxProperties(u8 bankId, u8 entryIdx, u8 channelIdx) {
    f32 vol = 1.0f;
    s8 volS8;
    s8 reverb = 0;
    f32 freqScale = 1.0f;
    s8 pan = 0x40;
    u8 stereoBits = 0;
    u8 filter = 0;
    s8 combFilterGain = 0;
    f32 behindScreenZ = 0.0f;
    u8 baseFilter = 0;
    OoTSoundBankEntry* entry = &sSoundBanks[bankId][entryIdx];
    s32 surround = (sSoundMode == SOUNDMODE_SURROUND_EXTERNAL);

    switch (bankId) {
        case OOT_BANK_PLAYER:
        case OOT_BANK_ITEM:
        case OOT_BANK_ENV:
        case OOT_BANK_ENEMY:
        case OOT_BANK_VOICE:
            if (surround) {
                combFilterGain = OoTSfx_ComputeHeightCombFilter(*entry->posY, entry->sfxParams);
            }
            // fallthrough
        case OOT_BANK_OCARINA:
            entry->dist = sqrtf(entry->dist);
            vol = OoTSfx_ComputeSoundVolume(bankId, entryIdx) * *entry->vol;
            reverb = OoTSfx_ComputeSoundReverb(bankId, entryIdx, channelIdx);
            pan = OoTSfx_ComputeSoundPanSigned(*entry->posX, *entry->posZ, entry->token);
            freqScale = OoTSfx_ComputeSoundFreqScale(bankId, entryIdx) * *entry->freqScale;
            if (surround) {
                behindScreenZ = sBehindScreenZ[(entry->sfxParams & 0x400) >> 10];
                if (!(entry->sfxParams & 0x800)) {
                    if (*entry->posZ < behindScreenZ) {
                        stereoBits = 0x10;
                    }
                    if ((sSfxChannelState[channelIdx].stereoBits ^ stereoBits) & 0x10) {
                        if (pan < 0x40) {
                            stereoBits = sSfxChannelState[channelIdx].stereoBits ^ 0x14;
                        } else {
                            stereoBits = sSfxChannelState[channelIdx].stereoBits ^ 0x18;
                        }
                    } else {
                        stereoBits = sSfxChannelState[channelIdx].stereoBits;
                    }
                }
            }
            if ((sAudioBaseFilter != 0) &&
                ((bankId == OOT_BANK_ITEM) || (bankId == OOT_BANK_PLAYER) || (bankId == OOT_BANK_VOICE))) {
                baseFilter = sAudioBaseFilter;
            }
            if ((baseFilter | sAudioExtraFilter) != 0) {
                filter = (baseFilter | sAudioExtraFilter);
            } else if (surround && ((entry->sfxParams & 0x2000) == 0)) {
                filter = OoTSfx_ComputeBehindScreenFilter(behindScreenZ, entry, pan);
            }
            break;
        default:
            break;
    }

    if (sSfxChannelState[channelIdx].vol != vol) {
        volS8 = (u8)(vol * 127.0f);
        sSfxChannelState[channelIdx].vol = vol;
    } else {
        volS8 = -1;
    }
    // io port 2: the volume
    AUDIOCMD_CHANNEL_SET_IO(OOT_PLAYER, channelIdx, 2, volS8);
    if (reverb != sSfxChannelState[channelIdx].reverb) {
        AUDIOCMD_CHANNEL_SET_REVERB_VOLUME(OOT_PLAYER, channelIdx, reverb);
        sSfxChannelState[channelIdx].reverb = reverb;
    }
    if (freqScale != sSfxChannelState[channelIdx].freqScale) {
        AUDIOCMD_CHANNEL_SET_FREQ_SCALE(OOT_PLAYER, channelIdx, freqScale);
        sSfxChannelState[channelIdx].freqScale = freqScale;
    }
    if (stereoBits != sSfxChannelState[channelIdx].stereoBits) {
        AUDIOCMD_CHANNEL_SET_STEREO(OOT_PLAYER, channelIdx, stereoBits | 0x10);
        sSfxChannelState[channelIdx].stereoBits = stereoBits;
    }
    if (filter != sSfxChannelState[channelIdx].filter) {
        // io port 3: the filter
        AUDIOCMD_CHANNEL_SET_IO(OOT_PLAYER, channelIdx, 3, filter);
        sSfxChannelState[channelIdx].filter = filter;
    }
    if (combFilterGain != (s8)sSfxChannelState[channelIdx].unk_0C) {
        AUDIOCMD_CHANNEL_SET_COMB_FILTER_SIZE(OOT_PLAYER, channelIdx, 0x10);
        AUDIOCMD_CHANNEL_SET_COMB_FILTER_GAIN(OOT_PLAYER, channelIdx, ((u16)(combFilterGain) << 8) + 0xFF);
        sSfxChannelState[channelIdx].unk_0C = combFilterGain;
    }
    if (pan != sSfxChannelState[channelIdx].pan) {
        AUDIOCMD_CHANNEL_SET_PAN(OOT_PLAYER, channelIdx, pan);
        sSfxChannelState[channelIdx].pan = pan;
    }
}

// Audio_PlayActiveSounds
static void OoTSfx_PlayActiveSounds(u8 bankId) {
    u8 entryIndex;
    SequenceChannel* channel;
    OoTSoundBankEntry* entry;
    u8 i;

    for (i = 0; i < sChannelsPerBank[bankId]; i++) {
        entryIndex = sActiveSounds[bankId][i].entryIndex;
        if (entryIndex != 0xFF) {
            entry = &sSoundBanks[bankId][entryIndex];
            channel = gAudioCtx.seqPlayers[OOT_PLAYER].channels[sCurSfxPlayerChannelIdx];
            if (entry->state == OOT_SFX_STATE_READY) {
                entry->channelIdx = sCurSfxPlayerChannelIdx;
                switch (entry->sfxParams & 0xC0) {
                    case 0x40:
                        entry->unk_2F = AudioThread_NextRandom() & 0xF;
                        break;
                    case 0x80:
                        entry->unk_2F = AudioThread_NextRandom() & 0x1F;
                        break;
                    case 0xC0:
                        entry->unk_2F = AudioThread_NextRandom() & 0x3F;
                        break;
                    default:
                        entry->unk_2F = 0;
                        break;
                }
                OoTSfx_SetSfxProperties(bankId, entryIndex, sCurSfxPlayerChannelIdx);
                AUDIOCMD_CHANNEL_SET_IO(OOT_PLAYER, sCurSfxPlayerChannelIdx, 0, 1);
                AUDIOCMD_CHANNEL_SET_IO(OOT_PLAYER, sCurSfxPlayerChannelIdx, 4, entry->sfxId & 0xFF);
                if (sIsLargeSoundBank[bankId]) {
                    AUDIOCMD_CHANNEL_SET_IO(OOT_PLAYER, sCurSfxPlayerChannelIdx, 5, (entry->sfxId & 0x100) >> 8);
                }
                if (entry->sfxId & 0xC00) {
                    entry->state = OOT_SFX_STATE_PLAYING_1;
                } else {
                    entry->state = OOT_SFX_STATE_PLAYING_2;
                }
            } else if ((u8)channel->seqScriptIO[1] == 0xFF) {
                OoTSfx_RemoveSoundBankEntry(bankId, entryIndex);
            } else if (entry->state == OOT_SFX_STATE_PLAYING_REFRESH) {
                OoTSfx_SetSfxProperties(bankId, entryIndex, sCurSfxPlayerChannelIdx);
                if (entry->sfxId & 0xC00) {
                    entry->state = OOT_SFX_STATE_PLAYING_1;
                } else {
                    entry->state = OOT_SFX_STATE_PLAYING_2;
                }
            }
        }
        sCurSfxPlayerChannelIdx++;
    }
}

// func_800F8884: a bank's sounds at a position stopped
static void OoTSfx_StopByPosInBank(u8 bankId, Vec3f* pos) {
    OoTSoundBankEntry* entry;
    u8 entryIndex = sSoundBanks[bankId][0].next;
    u8 prevEntryIndex = 0;

    while (entryIndex != 0xFF) {
        entry = &sSoundBanks[bankId][entryIndex];
        if (entry->posX == &pos->x) {
            if (entry->state >= OOT_SFX_STATE_PLAYING_REFRESH) {
                OoTSfx_StopChannel(entry->channelIdx);
            }
            if (entry->state != OOT_SFX_STATE_EMPTY) {
                OoTSfx_RemoveSoundBankEntry(bankId, entryIndex);
            }
        } else {
            prevEntryIndex = entryIndex;
        }
        entryIndex = sSoundBanks[bankId][prevEntryIndex].next;
    }
}

// Audio_StopSfxByPos
void MergeShip_OoTSfx_StopByPos(Vec3f* pos) {
    u8 i;
    OoTSoundBankEntry cmp;

    if (!sResetDone) {
        return;
    }
    for (i = 0; i < ARRAY_COUNT(sSoundBanks); i++) {
        OoTSfx_StopByPosInBank(i, pos);
    }
    cmp.posX = &pos->x;
    OoTSfx_RemoveMatchingSoundRequests(2, &cmp);
}

// ---- SoundSource_PlaySfxAtFixedWorldPos (OoT z_sound_source.c) ---------------------------------------------------

typedef struct {
    Vec3f worldPos;
    Vec3f projectedPos;
    u16 countdown;
} OoTSoundSource;

static OoTSoundSource sSoundSources[16];

static void OoTSfx_UpdateSoundSources(void) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sSoundSources); i++) {
        OoTSoundSource* source = &sSoundSources[i];

        if (source->countdown == 0) {
            continue;
        }
        if (--source->countdown == 0) {
            MergeShip_OoTSfx_StopByPos(&source->projectedPos);
        } else if (gPlayState != NULL) {
            SkinMatrix_Vec3fMtxFMultXYZ(&gPlayState->viewProjectionMtxF, &source->worldPos, &source->projectedPos);
        }
    }
}

// ---- the sequence player --------------------------------------------------------------------------------------------

// func_800F8F88, after Audio_ProcessSoundRequests: once a game frame, from MM's Audio_Update (code_8019AF00.c).
void MergeShip_OoTSfx_AudioUpdate(void) {
    SequencePlayer* seqPlayer = &gAudioCtx.seqPlayers[OOT_PLAYER];
    u8 bankId;

    if (gAudioCtx.audioBufferParameters.numSequencePlayers <= OOT_PLAYER) {
        return;
    }
    if (!sResetDone) {
        OoTSfx_ResetSounds();
    }
    OoTSfx_UpdateSoundSources();
    // the sequence: started when there is none (MM's audio up, or after a reset disabled it)
    if (!seqPlayer->enabled) {
        u16 seqId = MergeShip_Audio_OoTSeqId(0);

        if ((seqId != 0xFFFF) && (--sStartCooldown <= 0)) {
            AUDIOCMD_GLOBAL_INIT_SEQPLAYER(OOT_PLAYER, seqId, 0);
            OoTSfx_ResetSounds();
            sStartCooldown = 30;
            MergeShipTest_Log("OOTSFX player: Ocarina of Time's sequence 0 as MM sequence %d on player %d", seqId,
                              OOT_PLAYER);
        }
        // requests wait for it
        return;
    }
    // MM's sound effect volume (audio_thread_manager.c sets SEQ_PLAYER_SFX's) for these too
    if (seqPlayer->portVolumeScale != gAudioCtx.seqPlayers[SEQ_PLAYER_SFX].portVolumeScale) {
        seqPlayer->portVolumeScale = gAudioCtx.seqPlayers[SEQ_PLAYER_SFX].portVolumeScale;
        seqPlayer->recalculateVolume = true;
    }
    while (sSoundRequestWriteIndex != sSoundRequestReadIndex) {
        OoTSfx_ProcessSoundRequest();
        sSoundRequestReadIndex++;
    }
    if (IS_SEQUENCE_CHANNEL_VALID(seqPlayer->channels[0])) {
        sCurSfxPlayerChannelIdx = 0;
        for (bankId = 0; bankId < ARRAY_COUNT(sSoundBanks); bankId++) {
            OoTSfx_ChooseActiveSounds(bankId);
            OoTSfx_PlayActiveSounds(bankId);
        }
    }
    // the harness's evidence: how many notes this player sounds (a no-op outside the harness)
    {
        static s32 sLastNotes = 0;
        s32 notes = 0;
        s32 i;

        for (i = 0; (gAudioCtx.notes != NULL) && (i < gAudioCtx.numNotes); i++) {
            SequenceLayer* layer = gAudioCtx.notes[i].playbackState.parentLayer;

            if ((layer != NULL) && (layer != NO_LAYER) && (layer->channel != NULL) &&
                (layer->channel->seqPlayer == seqPlayer)) {
                notes++;
            }
        }
        if (notes != sLastNotes) {
            MergeShipTest_Log("OOTSFX notes %d", notes);
            sLastNotes = notes;
        }
    }
}

// ---- MergeShipOoTSfx.h -------------------------------------------------------------------------------------------

void MergeShip_OoTSfx_Actor(Actor* actor, u16 ootSfxId) {
    MergeShipTest_Log("OOTSFX 0x%04X", ootSfxId);
    OoTSfx_PlayGeneral(ootSfxId, &actor->projectedPos, 4, &sSfxDefaultFreqAndVolScale, &sSfxDefaultFreqAndVolScale,
                       &sSfxDefaultReverb);
}

void MergeShip_OoTSfx_ActorLoop(Actor* actor, u16 ootSfxId) {
    OoTSfx_PlayGeneral(ootSfxId - OOT_SFX_FLAG, &actor->projectedPos, 4, &sSfxDefaultFreqAndVolScale,
                       &sSfxDefaultFreqAndVolScale, &sSfxDefaultReverb);
}

void MergeShip_OoTSfx_Centered(u16 ootSfxId) {
    MergeShipTest_Log("OOTSFX 0x%04X", ootSfxId);
    OoTSfx_PlayGeneral(ootSfxId, &sSfxDefaultPos, 4, &sSfxDefaultFreqAndVolScale, &sSfxDefaultFreqAndVolScale,
                       &sSfxDefaultReverb);
}

void MergeShip_OoTSfx_CenteredLoop(u16 ootSfxId) {
    OoTSfx_PlayGeneral(ootSfxId - OOT_SFX_FLAG, &sSfxDefaultPos, 4, &sSfxDefaultFreqAndVolScale,
                       &sSfxDefaultFreqAndVolScale, &sSfxDefaultReverb);
}

void MergeShip_OoTSfx_AtPos(PlayState* play, Vec3f* pos, s32 duration, u16 ootSfxId) {
    s32 countdown = 0xFFFF;
    s32 smallest = 0;
    s32 i;
    OoTSoundSource* source;

    MergeShipTest_Log("OOTSFX 0x%04X", ootSfxId);
    // SoundSource_PlaySfxAtFixedWorldPos: a free source, else the one closest to running out
    for (i = 0; i < ARRAY_COUNT(sSoundSources); i++) {
        if (sSoundSources[i].countdown == 0) {
            smallest = i;
            break;
        }
        if (sSoundSources[i].countdown < countdown) {
            countdown = sSoundSources[i].countdown;
            smallest = i;
        }
    }
    source = &sSoundSources[smallest];
    if (source->countdown != 0) {
        MergeShip_OoTSfx_StopByPos(&source->projectedPos);
    }
    source->worldPos = *pos;
    source->countdown = duration;
    SkinMatrix_Vec3fMtxFMultXYZ(&play->viewProjectionMtxF, &source->worldPos, &source->projectedPos);
    OoTSfx_PlayGeneral(ootSfxId, &source->projectedPos, 4, &sSfxDefaultFreqAndVolScale, &sSfxDefaultFreqAndVolScale,
                       &sSfxDefaultReverb);
}
